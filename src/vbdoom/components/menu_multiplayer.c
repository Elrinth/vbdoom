/*
 * menu_multiplayer.c -- Multiplayer HOST/JOIN menu, connection handshake,
 * and level/mode selection for VB Doom.
 *
 * Uses the VB system font (loaded into CharSeg3) and vbPrint for text.
 */

#include <libgccvb.h>
#include <controller.h>
#include <misc.h>
#include "menu_multiplayer.h"
#include "../functions/link.h"
#include "../functions/sndplay.h"
#include "../functions/timer.h"
#include "../assets/audio/doom_sfx.h"

/* Font tile data from libgccvb-Barebone (256 tiles, 4096 bytes) */
#include "../assets/images/font_tiles.h"

/* ---- Helpers ---- */

/* Load font into CharSeg3 upper half (chars 1792-2047) so vbPrint works. */
static void loadFont(void) {
    copymem((void*)(CharSeg3 + 0x1000), (void*)FontTiles, 4096);
}

/* Set up a single text-display world using BGMap(0). */
static void setupTextWorld(void) {
    /* Clear all BGMaps we'll use */
    setmem((void*)BGMap(0), 0, 8192);

    /* World 31: Normal text layer, BGMap 0, full screen */
    vbSetWorld(31, WRLD_ON | 0, 0, 0, 0, 0, 0, 0, 384, 224);
    /* End marker */
    WA[30].head = WRLD_END;

    /* Palette: normal bright */
    VIP_REGS[GPLT0] = 0xE4;  /* 11 10 01 00 = bright to dark */
    VIP_REGS[GPLT1] = 0x00;
    VIP_REGS[GPLT2] = 0xE4;
    VIP_REGS[GPLT3] = 0xE4;
}

/* Clear the text BGMap. */
static void clearText(void) {
    setmem((void*)BGMap(0), 0, 8192);
}

/* Print a string at tile position (col, row) on BGMap 0 with palette 0. */
static void menuPrint(u16 col, u16 row, char *str) {
    vbPrint(0, col, row, str, 0);
}

/* Host debug: buffer and hex table for "P2 RX: 0x??" */
static char hostRxBuf[] = "P2 RX: 0x00";
static char hostAttemptBuf[] = "Try: 00";
static char joinFrameBuf[] = "Frame: 00";
static const char hex[] = "0123456789ABCDEF";

/* Draw cursor ">" at a specific row, col 4. Clear previous positions. */
static void drawCursor(u8 row, u8 numItems, u8 startRow) {
    u8 i;
    for (i = 0; i < numItems; i++) {
        /* Clear old cursor position */
        vbPrint(0, 4, startRow + i * 2, " ", 0);
    }
    vbPrint(0, 4, startRow + row * 2, ">", 0);
}

/* Wait until all buttons released (keep music alive). */
static void waitRelease(void) {
    u16 keys;
    keys = vbReadPad();
    while (keys) {
        updateMusic(true);
        vbWaitFrame(0);
        keys = vbReadPad();
    }
}

/* ---- Sub-screens ---- */

/* CONNECT/BACK selection. Returns 0=CONNECT, 255=BACK. */
static u8 screenConnectBack(void) {
    u8 pos = 0;
    u8 allowInput = 0;
    u16 keys;

    clearText();
    menuPrint(8, 3, "MULTIPLAYER");
    menuPrint(8, 6, "--------");
    menuPrint(4, 7, "BOTH: CHOOSE CONNECT");
    menuPrint(6, 9, "CONNECT");
    menuPrint(6, 11, "BACK");
    drawCursor(0, 2, 9);

    while (1) {
        keys = vbReadPad();

        if (keys & (K_A)) {
            if (allowInput) {
                playPlayerSFX(SFX_PISTOL);
                if (pos == 1) return 255;  /* BACK */
                return 0;  /* CONNECT */
            }
        } else if (keys & K_B) {
            if (allowInput) return 255;
        } else if (keys & (K_RU | K_LU)) {
            if (allowInput && pos > 0) {
                pos--;
                drawCursor(pos, 2, 9);
                playPlayerSFX(SFX_ELEVATOR_STP);
                allowInput = 0;
            }
        } else if (keys & (K_RD | K_LD)) {
            if (allowInput && pos < 1) {
                pos++;
                drawCursor(pos, 2, 9);
                playPlayerSFX(SFX_ELEVATOR_STP);
                allowInput = 0;
            }
        } else {
            allowInput = 1;
        }

        updateMusic(true);
        vbWaitFrame(0);
    }
}

/* Unified connecting screen. Both sides run same loop; host/joiner decided by link.
 * Returns 1=host, 0=joiner, 255=cancel/timeout. No linkInit here (done at menu entry). */
static u8 screenConnecting(void) {
    u16 timer = 0;
    u8 dotCount = 0;
    u8 dotTimer = 0;
    u8 recvByte;
    u16 keys;
    u8 allowInput = 0;
    #define CONNECT_TIMEOUT_FRAMES 600  /* ~30 s at 20fps */

    clearText();
    menuPrint(6, 5, "CONNECTING...");
    menuPrint(6, 9, "PRESS B TO CANCEL");

    while (timer < CONNECT_TIMEOUT_FRAMES) {
        keys = vbReadPad();
        if (keys & K_B && allowInput) return 255;
        if (!(keys & K_B)) allowInput = 1;

        /* Recv first (longer window so joiner is in recv when host sends HOST_ACK): 3 attempts per frame */
        {
            u8 attempt;
            for (attempt = 0; attempt < 3; attempt++) {
                if (linkTryRecvByte(&recvByte, 1000) == LINK_OK) {
                    if (recvByte == LINK_JOIN_REQ) {
                        /* Delay so other side can enter its recv phase next frame */
                        { u8 i; for (i = 0; i < 5; i++) { updateMusic(true); vbWaitFrame(0); } }
                        /* Resend HOST_ACK 3x so joiner has several recv windows to catch it */
                        linkSendByte(LINK_HOST_ACK);
                        { u8 i; for (i = 0; i < 2; i++) { updateMusic(true); vbWaitFrame(0); } }
                        linkSendByte(LINK_HOST_ACK);
                        { u8 i; for (i = 0; i < 2; i++) { updateMusic(true); vbWaitFrame(0); } }
                        linkSendByte(LINK_HOST_ACK);
                        return 1;  /* host */
                    }
                    if (recvByte == LINK_HOST_ACK) {
                        return 0;  /* joiner */
                    }
                }
            }
        }

        /* Then send so the other side can see us */
        linkTrySendByte(LINK_JOIN_REQ, 80);

        dotTimer++;
        if (dotTimer > 10) {
            dotTimer = 0;
            dotCount = (dotCount + 1) & 3;
            menuPrint(18, 5, "    ");
            if (dotCount >= 1) menuPrint(18, 5, ".");
            if (dotCount >= 2) menuPrint(19, 5, ".");
            if (dotCount >= 3) menuPrint(20, 5, ".");
        }

        updateMusic(true);
        vbWaitFrame(0);
        timer++;
    }

    /* Timeout */
    clearText();
    menuPrint(6, 7, "NO LINK CABLE?");
    menuPrint(6, 10, "PRESS ANY KEY");
    while (1) {
        keys = vbReadPad();
        if (keys & (K_A | K_STA | K_B)) break;
        updateMusic(true);
        vbWaitFrame(0);
    }
    return 255;
}

/* HOST waiting screen. Returns 1 on connection, 0 on cancel. */
static u8 screenHostWait(void) {
    u8 allowInput = 0;
    u16 keys;
    u8 dotCount = 0;
    u8 dotTimer = 0;
    u8 recvByte;

    clearText();
    menuPrint(6, 5, "WAITING FOR");
    menuPrint(6, 7, "OTHER PLAYER");
    menuPrint(6, 13, "CANCEL");
    drawCursor(0, 1, 13);

    linkInit();

    while (1) {
        keys = vbReadPad();

        /* Check for cancel */
        if (keys & (K_A | K_B)) {
            if (allowInput) {
                return 0;  /* cancelled */
            }
        } else {
            allowInput = 1;
        }

        /* Try to receive join request (non-blocking, short timeout) */
        if (linkTryRecvByte(&recvByte, 5000) == LINK_OK) {
            if (recvByte == LINK_JOIN_REQ) {
                /* Got join request! Send ack. */
                linkSendByte(LINK_HOST_ACK);
                return 1;  /* connected */
            }
        }

        /* Animate dots */
        dotTimer++;
        if (dotTimer > 10) {
            dotTimer = 0;
            dotCount = (dotCount + 1) & 3;
            menuPrint(18, 7, "    ");  /* clear dots area */
            if (dotCount >= 1) menuPrint(18, 7, ".");
            if (dotCount >= 2) menuPrint(19, 7, ".");
            if (dotCount >= 3) menuPrint(20, 7, ".");
        }

        updateMusic(true);
        vbWaitFrame(0);
    }
}

/* JOIN search screen (legacy; unused when using unified screen). Returns 1 on connection, 0 on timeout. */
static u8 screenJoinSearch(void) {
    u16 timer = 0;
    u8 dotCount = 0;
    u8 dotTimer = 0;
    u8 recvByte;
    u16 keys;
    u8 allowInput = 0;
    #define JOIN_TIMEOUT_FRAMES 200

    clearText();
    menuPrint(6, 5, "SEARCHING FOR");
    menuPrint(6, 7, "HOST");

    while (timer < JOIN_TIMEOUT_FRAMES) {
        /* Send join request, then check for ack */
        if (linkTrySendByte(LINK_JOIN_REQ, 5000) == LINK_OK) {
            /* Try to receive ack */
            if (linkTryRecvByte(&recvByte, 5000) == LINK_OK) {
                if (recvByte == LINK_HOST_ACK) {
                    return 1;  /* connected */
                }
            }
        }

        /* Animate dots */
        dotTimer++;
        if (dotTimer > 10) {
            dotTimer = 0;
            dotCount = (dotCount + 1) & 3;
            menuPrint(10, 7, "    ");
            if (dotCount >= 1) menuPrint(10, 7, ".");
            if (dotCount >= 2) menuPrint(11, 7, ".");
            if (dotCount >= 3) menuPrint(12, 7, ".");
        }

        updateMusic(true);
        vbWaitFrame(0);
        timer++;
    }

    /* Timeout */
    clearText();
    menuPrint(6, 7, "NO HOST FOUND");
    menuPrint(6, 10, "PRESS ANY KEY");

    while (1) {
        keys = vbReadPad();
        if (keys & (K_A | K_STA | K_B)) {
            break;
        }
        updateMusic(true);
        vbWaitFrame(0);
    }

    return 0;
}

/* Level and mode selection (HOST). Joiner already connected via unified screen.
 * Returns level number (1-4, 7=DM), or 0 on cancel. Sets g_gameMode. */
static u8 screenLevelMode(void) {
    u8 levelPos = 0;  /* 0=E1M1, 1=E1M2, 2=E1M3, 3=E1M4, 4=DM ARENA */
    u8 modePos = 0;   /* 0=COOP, 1=DEATHMATCH */
    u8 section = 0;   /* 0=level select, 1=mode select */
    u8 allowInput = 0;
    u8 joinerConnected = 1;  /* set by unified screen before we get here */
    u8 recvByte;
    u16 keys;
    u8 levelNums[] = {1, 2, 3, 4, 7};  /* actual level IDs */

    clearText();
    menuPrint(8, 2, "SELECT LEVEL");
    menuPrint(6, 5,  "E1M1");
    menuPrint(6, 7,  "E1M2");
    menuPrint(6, 9,  "E1M3");
    menuPrint(6, 11, "E1M4");
    menuPrint(6, 13, "DM ARENA");
    drawCursor(0, 5, 5);
    menuPrint(6, 17, "P2 CONNECTED!");
    menuPrint(6, 19, "WAITING P2... ");

    while (1) {
        keys = vbReadPad();

        if (section == 0) {
            /* Level selection */
            if (keys & (K_A)) {
                if (allowInput) {
                    playPlayerSFX(SFX_PISTOL);
                    /* Move to mode selection */
                    section = 1;
                    modePos = 0;
                    clearText();
                    menuPrint(8, 2, "SELECT MODE");
                    menuPrint(6, 6, "COOP");
                    menuPrint(6, 8, "DEATHMATCH");
                    drawCursor(0, 2, 6);
                    if (joinerConnected)
                        menuPrint(6, 12, "P2 CONNECTED!");
                    else
                        menuPrint(6, 12, "WAITING P2...");
                    menuPrint(6, 19, "WAITING P2... ");
                    allowInput = 0;
                }
            } else if (keys & K_B) {
                if (allowInput) return 0;
            } else if (keys & (K_RU | K_LU)) {
                if (allowInput && levelPos > 0) {
                    levelPos--;
                    drawCursor(levelPos, 5, 5);
                    playPlayerSFX(SFX_ELEVATOR_STP);
                    allowInput = 0;
                }
            } else if (keys & (K_RD | K_LD)) {
                if (allowInput && levelPos < 4) {
                    levelPos++;
                    drawCursor(levelPos, 5, 5);
                    playPlayerSFX(SFX_ELEVATOR_STP);
                    allowInput = 0;
                }
            } else {
                allowInput = 1;
            }
        } else {
            /* Mode selection -- confirm only allowed when P2 is connected */
            if (keys & (K_A )) {
                if (allowInput && joinerConnected) {
                    playPlayerSFX(SFX_PISTOL);
                    g_gameMode = modePos;  /* 0=COOP, 1=DEATHMATCH */
                    return levelNums[levelPos];
                }
            } else if (keys & K_B) {
                if (allowInput) {
                    /* Go back to level selection */
                    section = 0;
                    clearText();
                    menuPrint(8, 2, "SELECT LEVEL");
                    menuPrint(6, 5,  "E1M1");
                    menuPrint(6, 7,  "E1M2");
                    menuPrint(6, 9,  "E1M3");
                    menuPrint(6, 11, "E1M4");
                    menuPrint(6, 13, "DM ARENA");
                    drawCursor(levelPos, 5, 5);
                    if (joinerConnected)
                        menuPrint(6, 17, "P2 CONNECTED!");
                    else
                        menuPrint(6, 17, "WAITING P2...");
                    menuPrint(6, 19, "WAITING P2... ");
                    allowInput = 0;
                }
            } else if (keys & (K_RU | K_LU)) {
                if (allowInput && modePos > 0) {
                    modePos--;
                    drawCursor(modePos, 2, 6);
                    playPlayerSFX(SFX_ELEVATOR_STP);
                    allowInput = 0;
                }
            } else if (keys & (K_RD | K_LD)) {
                if (allowInput && modePos < 1) {
                    modePos++;
                    drawCursor(modePos, 2, 6);
                    playPlayerSFX(SFX_ELEVATOR_STP);
                    allowInput = 0;
                }
            } else {
                allowInput = 1;
            }
        }

        /* Link: try recv from joiner, update status line, send heartbeat */
        if (linkTryRecvByte(&recvByte, 500) == LINK_OK) {
            if (recvByte == LINK_JOINER_READY)
                menuPrint(6, 19, "P2 READY!     ");
            else
                menuPrint(6, 19, "P2 RX OK      ");
        } else {
            menuPrint(6, 19, "WAITING P2... ");
        }
        if (section == 0)
            linkTrySendByte(LINK_HOST_LEVEL_BASE + levelPos, 300);
        else
            linkTrySendByte(LINK_HOST_MODE_BASE + modePos, 300);

        updateMusic(true);
        vbWaitFrame(0);
    }
}

/* Host selection display strings (for joiner status line) */
static const char * const joinLevelStr[] = { "HOST: E1M1 ", "HOST: E1M2 ", "HOST: E1M3 ", "HOST: E1M4 ", "HOST: DM   " };
static const char * const joinModeStr[] = { "HOST: COOP ", "HOST: DM   " };

/* JOIN waits for HOST to send level + mode. Returns level number or 0 on error.
 * Tries recv first (heartbeat/selection), then sends JOINER_READY, then recv for GAME_START. */
static u8 screenJoinWaitForLevel(void) {
    u8 levelNum;
    u8 levelNum2;
    u8 mode;
    u8 byte;
    u16 keys;
    u8 allowInput = 0;
    u8 joinFrameCount = 0;

    clearText();
    menuPrint(6, 7, "WAITING FOR HOST");
    menuPrint(6, 9, "TO SELECT LEVEL");
    menuPrint(6, 11, "LINK: ---      ");
    menuPrint(6, 15, "B = CANCEL");

    byte = HW_REGS[CDRR];   /* discard leftover byte from handshake (e.g. HOST_ACK) */

    while (1) {
        joinFrameCount = (joinFrameCount + 1) % 100;
        joinFrameBuf[7] = '0' + (joinFrameCount / 10);
        joinFrameBuf[8] = '0' + (joinFrameCount % 10);
        menuPrint(6, 17, joinFrameBuf);
        keys = vbReadPad();
        if (keys & K_B && allowInput) return 0;  /* cancel */
        if (!(keys & K_B)) allowInput = 1;

        /* Recv first: host heartbeat/selection or GAME_START */
        if (linkTryRecvByte(&byte, 12000) == LINK_OK) {
            if (byte == LINK_GAME_START)
                break;
            if (byte >= LINK_HOST_LEVEL_BASE && byte <= LINK_HOST_LEVEL_BASE + 4)
                menuPrint(6, 11, joinLevelStr[byte - LINK_HOST_LEVEL_BASE]);
            else if (byte == LINK_HOST_MODE_BASE)
                menuPrint(6, 11, joinModeStr[0]);
            else if (byte == LINK_HOST_MODE_BASE + 1)
                menuPrint(6, 11, joinModeStr[1]);
            else if (byte == LINK_HOST_LISTENING)
                menuPrint(6, 11, "HOST READY    ");
            else if (byte == 1 || byte == 2 || byte == 3 || byte == 4 || byte == 7) {
                /* Missed GAME_START; this byte is first level */
                levelNum = byte;
                levelNum2 = linkRecvByte();
                if (levelNum != levelNum2) return 0;
                mode = linkRecvByte();
                goto got_level_mode;
            }
            else
                menuPrint(6, 11, "LINK OK       ");
        }
        menuPrint(6, 13, "SENDING TO HOST ");
        /* Send JOINER_READY many times so host (in recv) can catch us */
        {
            u8 k;
            for (k = 0; k < 30; k++) {
                linkTrySendByte(LINK_JOINER_READY, 1000);
            }
        }
        /* Block until we receive one byte (host sends GAME_START) */
        byte = linkRecvByte();
        if (byte == LINK_GAME_START)
            break;
        /* Missed GAME_START: host already sent it and we got first level byte here */
        if (byte == 1 || byte == 2 || byte == 3 || byte == 4 || byte == 7) {
            levelNum = byte;
            levelNum2 = linkRecvByte();
            if (levelNum != levelNum2) return 0;
            mode = linkRecvByte();
            goto got_level_mode;
        }
        updateMusic(true);
        vbWaitFrame(0);
    }

got_game_start:

    menuPrint(6, 11, "GAME START RECEIVED!");
    menuPrint(6, 13, "LOADING...");

    /* Short delay so host is in linkSendByte(levelNum) before we recv */
    { u8 i; for (i = 0; i < 5; i++) { updateMusic(true); vbWaitFrame(0); } }

    /* Receive level (twice), then mode; verify level bytes match */
    levelNum = linkRecvByte();
    levelNum2 = linkRecvByte();
    if (levelNum != levelNum2) return 0;
    mode = linkRecvByte();

got_level_mode:
    if (levelNum != 1 && levelNum != 2 && levelNum != 3 && levelNum != 4 && levelNum != 7)
        return 0;
    if (mode > 1)
        return 0;
    /* Deathmatch uses level 7 only; recover from corrupted level byte */
    if (mode == GAMEMODE_DEATHMATCH && levelNum != 7)
        levelNum = 7;

    g_gameMode = mode;

    /* Send sync ack (blocking: joiner waits until host receives) */
    linkSendByte(LINK_SYNC_OK);
    return levelNum;
}

/* ---- Main multiplayer menu entry ---- */

u8 multiplayerMenu(void) {
    u8 choice;
    u8 connectResult;
    u8 levelNum;

    /* Reset multiplayer state */
    g_isMultiplayer = false;
    g_isHost = false;
    g_gameMode = GAMEMODE_COOP;
    g_fragCount = 0;
    g_deathCount = 0;

    linkInit();  /* put EXT port in known link state (both host and joiner) */

    /* Setup display for text menus */
    vbDisplayHide();
    loadFont();
    setupTextWorld();
    vbDisplayOn();
    vbDisplayShow();

    while (1) {
        /* CONNECT / BACK */
        choice = screenConnectBack();
        if (choice == 255) return 0;  /* BACK -> title screen */

        /* Unified connecting: both sides run same loop; returns 1=host, 0=joiner, 255=cancel */
        connectResult = screenConnecting();
        if (connectResult == 255) continue;  /* cancel or timeout -> back to CONNECT/BACK */

        if (connectResult == 1) {
            /* HOST: joiner already connected */
            g_isHost = true;

            levelNum = screenLevelMode();
            if (levelNum == 0) continue;  /* cancelled -> back to HOST/JOIN */

            /* Give joiner time to enter wait screen */
            { u8 i; for (i = 0; i < 10; i++) { updateMusic(true); vbWaitFrame(0); } }

            /* Sync: host sends once (blocking) so joiner receives; then joiner does send JOINER_READY while host is in recv */
            linkSendByte(LINK_HOST_LISTENING);

            /* Try recv with status: show each byte received (hex) or "Waiting P2..."; accept any first byte */
            menuPrint(6, 19, "WAIT P2...     ");
            {
                u8 readyByte;
                u8 attempt;
                for (attempt = 0; attempt < 60; attempt++) {
                    hostAttemptBuf[5] = '0' + (attempt / 10);
                    hostAttemptBuf[6] = '0' + (attempt % 10);
                    menuPrint(6, 17, hostAttemptBuf);
                    if (linkTryRecvByte(&readyByte, 5000) == LINK_OK) {
                        hostRxBuf[9] = hex[readyByte >> 4];
                        hostRxBuf[10] = hex[readyByte & 0xF];
                        menuPrint(6, 19, hostRxBuf);
                        goto joiner_ready;
                    }
                    menuPrint(6, 19, "Waiting P2...  ");
                    updateMusic(true);
                    vbWaitFrame(0);
                }
            }

            /* Listen-only phase: many recv attempts over 20 frames so we overlap with joiner send */
            {
                u8 readyByte;
                u8 listenFrame;
                u8 listenAttempt;
                for (listenFrame = 0; listenFrame < 20; listenFrame++) {
                    hostAttemptBuf[5] = '0' + (listenFrame / 10);
                    hostAttemptBuf[6] = '0' + (listenFrame % 10);
                    menuPrint(6, 17, hostAttemptBuf);
                    for (listenAttempt = 0; listenAttempt < 20; listenAttempt++) {
                        if (linkTryRecvByte(&readyByte, 4000) == LINK_OK) {
                            hostRxBuf[9] = hex[readyByte >> 4];
                            hostRxBuf[10] = hex[readyByte & 0xF];
                            menuPrint(6, 19, hostRxBuf);
                            if (readyByte == LINK_JOINER_READY) goto joiner_ready;
                        }
                        updateMusic(true);
                    }
                    vbWaitFrame(0);
                }
            }
            /* Wait for joiner READY (more recv attempts and longer timeout per frame) */
            {
                u8 readyByte;
                u16 readyRetries = 0;
                u8 attempts;
                while (1) {
                    hostAttemptBuf[5] = '0' + ((readyRetries / 10) % 10);
                    hostAttemptBuf[6] = '0' + (readyRetries % 10);
                    menuPrint(6, 17, hostAttemptBuf);
                    for (attempts = 0; attempts < 20; attempts++) {
                        if (linkTryRecvByte(&readyByte, 4000) == LINK_OK) {
                            hostRxBuf[9] = hex[readyByte >> 4];
                            hostRxBuf[10] = hex[readyByte & 0xF];
                            menuPrint(6, 19, hostRxBuf);
                            if (readyByte == LINK_JOINER_READY) goto joiner_ready;
                        }
                        updateMusic(true);
                    }
                    vbWaitFrame(0);
                    if (++readyRetries > 300) goto show_joiner_not_responding;
                }
joiner_ready: ;
            }

            /* Brief delay so joiner is in recv window when we send */
            { u8 i; for (i = 0; i < 5; i++) { updateMusic(true); vbWaitFrame(0); } }

            /* Send GAME_START (blocking: host waits until joiner receives) */
            menuPrint(6, 19, "Sending...     ");
            linkSendByte(LINK_GAME_START);
            linkSendByte(levelNum);
            linkSendByte(levelNum);  /* send twice so joiner can verify */
            linkSendByte(g_gameMode);
            {
                u8 syncByte;
                syncByte = linkRecvByte();
sync_ok: ;
            }
            goto host_success;

show_joiner_not_responding:
            {
                u8 i;
                clearText();
                menuPrint(6, 9, "JOINER NOT RESPONDING");
                menuPrint(6, 11, "PRESS ANY KEY");
                linkInit();
                for (i = 0; i < 120; i++) { updateMusic(true); vbWaitFrame(0); }
                continue;  /* back to CONNECT/BACK menu */
            }
host_success: ;
        } else {
            /* JOIN: go straight to wait for level (handshake already done in screenConnecting) */
            g_isHost = false;
            levelNum = screenJoinWaitForLevel();
            if (levelNum == 0) continue;  /* error or cancel -> back to CONNECT/BACK */
        }

        break;  /* success -- proceed to game */
    }

    /* Short delay so confirm key often released before game; never block forever */
    { u8 i; for (i = 0; i < 10; i++) { updateMusic(true); vbWaitFrame(0); } }

    /* Connection established, settings agreed upon */
    g_isMultiplayer = true;
    g_player2Alive = true;
    g_player2Health = 100;

    /* Clean up display */
    vbDisplayHide();
    setmem((void*)BGMap(0), 0, 8192);

    return levelNum;
}
