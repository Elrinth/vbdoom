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

        /* Recv first: if other side sent JOIN_REQ we become host; if they sent HOST_ACK we become joiner */
        if (linkTryRecvByte(&recvByte, 80) == LINK_OK) {
            if (recvByte == LINK_JOIN_REQ) {
                linkSendByte(LINK_HOST_ACK);
                return 1;  /* host */
            }
            if (recvByte == LINK_HOST_ACK) {
                return 0;  /* joiner */
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

        updateMusic(true);
        vbWaitFrame(0);
    }
}

/* JOIN waits for HOST to send level + mode. Returns level number or 0 on error.
 * Sends JOINER_READY repeatedly until we receive GAME_START so host can receive
 * ready at any time after confirming level/mode. */
static u8 screenJoinWaitForLevel(void) {
    u8 levelNum;
    u8 mode;
    u8 byte;
    u16 keys;
    u8 allowInput = 0;

    clearText();
    menuPrint(6, 7, "WAITING FOR HOST");
    menuPrint(6, 9, "TO SELECT LEVEL");
    menuPrint(6, 15, "B = CANCEL");

    byte = HW_REGS[CDRR];   /* discard leftover byte from handshake (e.g. HOST_ACK) */

    /* Send JOINER_READY and check for GAME_START each iteration until host sends it */
    while (1) {
        keys = vbReadPad();
        if (keys & K_B && allowInput) return 0;  /* cancel */
        if (!(keys & K_B)) allowInput = 1;

        linkTrySendByte(LINK_JOINER_READY, 500);   /* advertise ready so host can recv anytime */
        if (linkTryRecvByte(&byte, 80) == LINK_OK && byte == LINK_GAME_START)
            break;

        updateMusic(true);
        vbWaitFrame(0);
    }
got_game_start:

    menuPrint(6, 11, "GAME START RECEIVED!");
    menuPrint(6, 13, "LOADING...");

    /* Receive level number */
    HW_REGS[CCR] = 0x94;  /* restart listening */
    while (HW_REGS[CCR] & 0x02) {}  /* wait for transfer */
    levelNum = HW_REGS[CDRR];

    /* Receive game mode */
    HW_REGS[CCR] = 0x94;  /* restart listening */
    while (HW_REGS[CCR] & 0x02) {}  /* wait for transfer */
    mode = HW_REGS[CDRR];

    g_gameMode = mode;

    /* Send sync ack */
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

            /* Wait for joiner READY before sending */
            {
                u8 readyByte;
                u16 readyRetries = 0;
                while (1) {
                    if (linkTryRecvByte(&readyByte, 5000) == LINK_OK && readyByte == LINK_JOINER_READY)
                        break;
                    updateMusic(true);
                    vbWaitFrame(0);
                    if (++readyRetries > 150) goto show_joiner_not_responding;
                }
            }

            /* Send each byte with retries */
            {
                u16 retries;
                retries = 0;
                while (linkTrySendByte(LINK_GAME_START, 5000) != LINK_OK) {
                    updateMusic(true);
                    vbWaitFrame(0);
                    if (++retries > 150) break;
                }
            }
            {
                u16 retries;
                retries = 0;
                while (linkTrySendByte(levelNum, 5000) != LINK_OK) {
                    updateMusic(true);
                    vbWaitFrame(0);
                    if (++retries > 150) break;
                }
            }
            {
                u16 retries;
                retries = 0;
                while (linkTrySendByte(g_gameMode, 5000) != LINK_OK) {
                    updateMusic(true);
                    vbWaitFrame(0);
                    if (++retries > 150) break;
                }
            }
            {
                u8 syncByte;
                u16 ackRetries = 0;
                while (1) {
                    if (linkTryRecvByte(&syncByte, 5000) == LINK_OK && syncByte == LINK_SYNC_OK)
                        break;  /* got sync ack, proceed to game */
                    updateMusic(true);
                    vbWaitFrame(0);
                    if (++ackRetries > 200) goto show_joiner_not_responding;
                }
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
