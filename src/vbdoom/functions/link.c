/*
 * link.c -- Virtual Boy Link Cable (EXT Port) communication
 *
 * Hardware registers at HW_REGS (0x02000000):
 *   CCR  (0x00) - Link Port Control 1
 *   CCSR (0x04) - init only (interrupt disable); not used per transfer (3D BattleSnake/Tron don't use it)
 *   CDTR (0x08) - Transmitted Data Register
 *   CDRR (0x0C) - Received Data Register
 *
 * CCR values (match 3D BattleSnake / Tron):
 *   0x80 - idle/reset
 *   0x84 - start transfer as master (drives clock)
 *   0x94 - start transfer as remote (receives clock)
 *   bit 1 (& 0x02) - transfer busy
 *
 * Pattern from 3D BattleSnake (tron.c): receiver must be in 0x94 before sender does 0x84;
 * sender uses a long delay (200000 cycles) so receiver is ready. No CCSR sync (not used there).
 */

#include <libgccvb.h>
#include "link.h"

/* 0 = host recv first then send, joiner send first then recv (3D BattleSnake / menu pattern). */
#define LINK_HOST_SENDS_FIRST 0

/* ---- Globals (multiplayer/link state; set by menu, read by gameLoop and link.c; do not shadow) ---- */
bool g_isMultiplayer = false;
bool g_isHost = false;
u8   g_gameMode = GAMEMODE_COOP;

u16  g_player2X = 0;
u16  g_player2Y = 0;
u16  g_player2Angle = 0;
u8   g_player2Flags = 0;
u8   g_player2Health = 0;
bool g_player2Alive = false;

u16  g_linkSendCount = 0;
u16  g_linkRecvCount = 0;
u8   g_linkTestRecv = 0;

u8   g_fragCount = 0;
u8   g_deathCount = 0;

u16  g_p2SpawnX = 0;
u16  g_p2SpawnY = 0;
u16  g_p2SpawnAngle = 0;

u8   g_p2AnimFrame = 0;
u8   g_p2AnimTimer = 0;
u8   g_p2LastFrame = 255;

/* ---- Multiplayer debug/recv state (single copy for HUD and exchange) ---- */
u16  g_mpFreshPacketCount = 0;
u16  g_mpRecvBufChangedCount = 0;
u8   g_mpLastRecvRaw[4] = {0, 0, 0, 0};
u8   g_mpLastApplied = 0;
u8   g_mpLastSame = 0;
u8   g_mpLastPollRecv = 0;

/* ---- Low-level ---- */

/* CCSR bit 2: VUEngine and rumble clear it to "open" the channel / signal ready; 0xFF leaves it set and can break link. */
#define CCSR_CHANNEL_READY 0x02  /* clear this bit to open channel (receiver or sender ready) */

void linkInit(void) {
    u16 i;
    HW_REGS[CCSR] = 0xFF;                       /* disable interrupts etc. */
    HW_REGS[CCSR] &= ~CCSR_CHANNEL_READY;       /* clear bit 2 = open channel (VUEngine/rumble) */
    HW_REGS[CCR] = 0x80;
    for (i = 0; i < 2000; i++) { (void)0; }
}

/* Discard any stale byte in CDRR (e.g. after menu handshake). Call once after linkInit() in game. */
void linkDiscardRecvByte(void) {
    (void)HW_REGS[CDRR];
}

void linkSendByte(u8 data) {
    u16 i;
    for (i = 0; i < 10000; i++) {   /* wait for port idle (LEMUR needs longer) */
        if (!(HW_REGS[CCR] & 0x02)) break;
    }
    HW_REGS[CCSR] &= ~CCSR_CHANNEL_READY;  /* open channel before send (rumble / VUEngine) */
    HW_REGS[CDTR] = data;
    HW_REGS[CCR] = 0x84;           /* start transfer as master (internal clock) */
    while (HW_REGS[CCR] & 0x02);   /* wait for completion */
    HW_REGS[CCR] = 0x80;           /* release port to idle */
    g_linkSendCount++;
}

u8 linkRecvByte(void) {
    u8 b;
    u16 i;
    for (i = 0; i < 10000; i++) {   /* wait for port idle (LEMUR needs longer) */
        if (!(HW_REGS[CCR] & 0x02)) break;
    }
    HW_REGS[CCSR] &= ~CCSR_CHANNEL_READY;  /* open channel / signal ready before listen (VUEngine remote) */
    HW_REGS[CCR] = 0x94;           /* start transfer as remote (TTT: no CDTR write) */
    while (HW_REGS[CCR] & 0x02);   /* wait for completion */
    b = HW_REGS[CDRR];
    HW_REGS[CCR] = 0x80;           /* release port to idle */
    g_linkRecvCount++;
    return b;
}

/* One-byte exchange: receiver in 0x94 first. Byte 0: joiner sends first, then host sends after delay so joiner is in recv. */
#define LINK_ONE_BYTE_HOST_DELAY    600000u  /* host waits so joiner is in linkRecvByte (bytes 1-7) */
#define LINK_ONE_BYTE_JOINER_DELAY   80000u  /* joiner waits after recv so host is in linkRecvByte */
#define LINK_ONE_BYTE_JOINER_SEND_FIRST 500000u   /* byte 0: joiner waits so host is in linkRecvByte before we send */
#define LINK_ONE_BYTE_HOST_AFTER_RECV 1000000u   /* byte 0: host waits after recv so joiner has entered linkRecvByte() before we send */

void linkExchangeOneByte(u8 toSend, u8 *recvOut) {
    static u8 s_byteIndex = 0;
    if (s_byteIndex == 0) {
        /* First byte: host recv (joiner send first), then host delay then send so joiner is in linkRecvByte() */
        if (g_isHost) {
            *recvOut = linkRecvByte();   /* wait for joiner's byte 0 */
            { u32 d; for (d = 0; d < LINK_ONE_BYTE_HOST_AFTER_RECV; d++) { (void)0; } }
            linkSendByte(toSend);        /* send our byte 0 - joiner is now in linkRecvByte() */
        } else {
            { u32 d; for (d = 0; d < LINK_ONE_BYTE_JOINER_SEND_FIRST; d++) { (void)0; } }
            linkSendByte(toSend);        /* send our byte 0 to host */
            *recvOut = linkRecvByte();   /* then get host's byte 0 - we are in recv when host sends */
        }
    } else {
        if (g_isHost) {
            { u32 d; for (d = 0; d < LINK_ONE_BYTE_HOST_DELAY; d++) { (void)0; } }
            linkSendByte(toSend);
            *recvOut = linkRecvByte();
        } else {
            *recvOut = linkRecvByte();
            { u32 d; for (d = 0; d < LINK_ONE_BYTE_JOINER_DELAY; d++) { (void)0; } }
            linkSendByte(toSend);
        }
    }
    s_byteIndex = (s_byteIndex + 1) & 7;
}

u8 linkTrySendByte(u8 data, u16 timeout) {
    u16 i;
    for (i = 0; i < 5000; i++) {   /* wait for port idle before starting */
        if (!(HW_REGS[CCR] & 0x02)) break;
    }
    HW_REGS[CCSR] &= ~CCSR_CHANNEL_READY;
    HW_REGS[CDTR] = data;
    HW_REGS[CCR] = 0x84;           /* start transfer as master */
    for (i = 0; i < timeout; i++) {
        if (!(HW_REGS[CCR] & 0x02)) {
            HW_REGS[CCR] = 0x80;           /* release port to idle */
            g_linkSendCount++;
            return LINK_OK;
        }
    }
    HW_REGS[CCR] = 0x80;           /* cancel / reset */
    return LINK_TIMEOUT;
}

u8 linkTryRecvByte(u8 *out, u16 timeout) {
    u16 i;
    for (i = 0; i < 5000; i++) {   /* wait for port idle before starting */
        if (!(HW_REGS[CCR] & 0x02)) break;
    }
    HW_REGS[CCSR] &= ~CCSR_CHANNEL_READY;
    HW_REGS[CCR] = 0x94;           /* start transfer as remote (TTT: no CDTR write) */
    for (i = 0; i < timeout; i++) {
        if (!(HW_REGS[CCR] & 0x02)) {
            *out = HW_REGS[CDRR];
            HW_REGS[CCR] = 0x80;           /* release port to idle */
            g_linkRecvCount++;
            return LINK_OK;
        }
    }
    HW_REGS[CCR] = 0x80;           /* cancel / reset */
    return LINK_TIMEOUT;
}

/* ---- Multi-byte ---- */

/* Delay before first byte so receiver is in 0x94 (LEMUR needs long delay for packet exchange). */
#define LINK_SEND_PACKET_INITIAL_DELAY 20000000u

void linkSendPacket(const u8 *buf, u8 len) {
    u8 i;
    u32 d;
    for (d = 0; d < LINK_SEND_PACKET_INITIAL_DELAY; d++) { (void)0; }
    for (i = 0; i < len; i++) {
        linkSendByte(buf[i]);
    }
}

void linkRecvPacket(u8 *buf, u8 len) {
    u8 i;
    for (i = 0; i < len; i++) {
        buf[i] = linkRecvByte();
    }
}

/* ---- High-level state exchange ---- */

/* Per Sacred Tech Scroll: "If the internal clock is selected and C-Start is set without another
 * unit waiting for an external clock, a peerless communication will be performed immediately
 * and the bits in CDRR will be undefined." So the RECEIVER must be waiting (CCR=0x94) before
 * the SENDER starts. We ensure that by delaying in linkSendByte so the other side reaches
 * linkRecvByte first. */

/* Full duplex: receiver must be in 0x94 before sender starts (delay in linkSendPacket).
 * Default: host recv first, joiner send first. LINK_HOST_SENDS_FIRST=1 swaps for LEMUR. */
void linkExchangeState(u8 *sendBuf, u8 *recvBuf) {
#if LINK_HOST_SENDS_FIRST
    if (g_isHost) {
        linkSendPacket(sendBuf, 8);
        linkRecvPacket(recvBuf, 8);
    } else {
        linkRecvPacket(recvBuf, 8);
        linkSendPacket(sendBuf, 8);
    }
#else
    if (g_isHost) {
        linkRecvPacket(recvBuf, 8);
        linkSendPacket(sendBuf, 8);
    } else {
        linkSendPacket(sendBuf, 8);
        linkRecvPacket(recvBuf, 8);
    }
#endif
}

/* Per-byte: receiver in 0x94 first, then sender delay+send. First byte: long delay so LEMUR has joiner in recv. */
#define LINK_BYTE_DELAY       30000u   /* after recv, before send (so other side can enter next recv) */
#define LINK_FIRST_BYTE_DELAY 2500000u /* host waits this before sending byte 0 so joiner is in linkRecvByte() */
#define LINK_JOINER_START_DELAY 200000u /* joiner waits so host enters first-byte delay before we go to recv */

void linkExchangeStateByteByByte(u8 *sendBuf, u8 *recvBuf) {
    u8 i;
    for (i = 0; i < 8; i++) {
        if (g_isHost) {
            { u32 d; for (d = 0; d < (i == 0 ? LINK_FIRST_BYTE_DELAY : LINK_BYTE_DELAY); d++) { (void)0; } }
            linkSendByte(sendBuf[i]);
            recvBuf[i] = linkRecvByte();
        } else {
            if (i == 0) { u32 d; for (d = 0; d < LINK_JOINER_START_DELAY; d++) { (void)0; } }
            recvBuf[i] = linkRecvByte();
            { u32 d; for (d = 0; d < LINK_BYTE_DELAY; d++) { (void)0; } }
            linkSendByte(sendBuf[i]);
        }
    }
}

/* Receiver must be in 0x94 before sender does 0x84. LEMUR needs long timeouts so both
 * sides have time to reach tryRecv/trySend in the same round. */
#define LINK_POLL_SENDER_DELAY      20000u   /* cycles for other side to enter 0x94 */
#define LINK_POLL_HOST_SEND_DELAY  100000u   /* host waits this before trySend so joiner is in tryRecv */
#define LINK_POLL_RECV_TIMEOUT     100000u   /* long so we're still waiting when they send */
#define LINK_POLL_SEND_TIMEOUT      20000u   /* sender wait for transfer complete */
#define LINK_POLL_ROUNDS            500u    /* more rounds to get 8 bytes each way per frame */

u8 linkExchangeStatePolling(u8 *sendBuf, u8 *recvBuf) {
    u8 recvCount = 0;
    u8 sendCount = 0;
    u16 round;
    for (round = 0; round < LINK_POLL_ROUNDS; round++) {
        if (g_isHost) {
            if (recvCount < 8 && linkTryRecvByte(recvBuf + recvCount, (u16)LINK_POLL_RECV_TIMEOUT) == LINK_OK)
                recvCount++;
            if (sendCount < 8) {
                { u32 d; for (d = 0; d < LINK_POLL_HOST_SEND_DELAY; d++) { (void)0; } }
                { u32 d; for (d = 0; d < LINK_POLL_SENDER_DELAY; d++) { (void)0; } }
                if (linkTrySendByte(sendBuf[sendCount], (u16)LINK_POLL_SEND_TIMEOUT) == LINK_OK)
                    sendCount++;
            }
        } else {
            if (sendCount < 8) {
                { u32 d; for (d = 0; d < LINK_POLL_SENDER_DELAY; d++) { (void)0; } }
                if (linkTrySendByte(sendBuf[sendCount], (u16)LINK_POLL_SEND_TIMEOUT) == LINK_OK)
                    sendCount++;
            }
            if (recvCount < 8 && linkTryRecvByte(recvBuf + recvCount, (u16)LINK_POLL_RECV_TIMEOUT) == LINK_OK)
                recvCount++;
        }
        if (recvCount >= 8 && sendCount >= 8)
            break;
    }
    return recvCount;
}
