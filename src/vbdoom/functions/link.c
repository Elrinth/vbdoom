/*
 * link.c -- Virtual Boy Link Cable (EXT Port) communication
 *
 * Hardware registers at HW_REGS (0x02000000):
 *   CCR  (0x00) - Link Port Control 1
 *   CCSR (0x04) - Link Port Control 2 (interrupt control)
 *   CDTR (0x08) - Transmitted Data Register
 *   CDRR (0x0C) - Received Data Register
 *
 * CCR values:
 *   0x80 - idle/reset
 *   0x84 - start transfer as master (drives clock)
 *   0x94 - start transfer as remote (receives clock)
 *   bit 1 (& 0x02) - transfer busy
 *
 * VUEngine/SPONG patterns: open channel (CCSR bit 1), wait for channel open (CCSR bit 0),
 * receiver writes CDTR before receive; close channel after transfer.
 */
#define LINK_CHAN_OPEN_TIMEOUT 100
#define LINK_RECV_CDTR_DUMMY  0x44  /* VUEngine __COM_CHECKSUM */

#include <libgccvb.h>
#include "link.h"

/* ---- Globals ---- */
bool g_isMultiplayer = false;
bool g_isHost = false;
u8   g_gameMode = GAMEMODE_COOP;

u16  g_player2X = 0;
u16  g_player2Y = 0;
u16  g_player2Angle = 0;
u8   g_player2Flags = 0;
u8   g_player2Health = 0;
bool g_player2Alive = false;

u8   g_fragCount = 0;
u8   g_deathCount = 0;

u16  g_p2SpawnX = 0;
u16  g_p2SpawnY = 0;
u16  g_p2SpawnAngle = 0;

u8   g_p2AnimFrame = 0;
u8   g_p2AnimTimer = 0;
u8   g_p2LastFrame = 255;

/* ---- Low-level ---- */

void linkInit(void) {
    u16 i;
    /* Disable link port interrupts so unready receiver doesn't cause issues */
    HW_REGS[CCSR] = 0xFF;
    /* Set link port to idle/ready state */
    HW_REGS[CCR] = 0x80;
    /* Short stabilization (VUEngine: wait for channel to stabilize) */
    for (i = 0; i < 2000; i++) { (void)0; }
}

void linkSendByte(u8 data) {
    u16 i;
    HW_REGS[CCSR] |= 0x02;   /* open channel */
    for (i = 0; i < LINK_CHAN_OPEN_TIMEOUT; i++) {
        if (HW_REGS[CCSR] & 0x01) break;
    }
    HW_REGS[CDTR] = data;
    HW_REGS[CCR] = 0x84;           /* start transfer as master */
    while (HW_REGS[CCR] & 0x02);   /* wait for completion */
    HW_REGS[CCSR] &= (u8)~0x02;    /* close channel */
}

u8 linkRecvByte(void) {
    u16 i;
    u8 b;
    HW_REGS[CCSR] |= 0x02;   /* open channel */
    for (i = 0; i < LINK_CHAN_OPEN_TIMEOUT; i++) {
        if (HW_REGS[CCSR] & 0x01) break;
    }
    HW_REGS[CDTR] = LINK_RECV_CDTR_DUMMY;  /* VUEngine: receiver writes before start */
    HW_REGS[CCR] = 0x94;           /* start transfer as remote */
    while (HW_REGS[CCR] & 0x02);   /* wait for completion */
    b = HW_REGS[CDRR];
    HW_REGS[CCSR] &= (u8)~0x02;    /* close channel */
    return b;
}

u8 linkTrySendByte(u8 data, u16 timeout) {
    u16 i;
    HW_REGS[CCSR] |= 0x02;   /* open channel */
    for (i = 0; i < LINK_CHAN_OPEN_TIMEOUT; i++) {
        if (HW_REGS[CCSR] & 0x01) break;
    }
    HW_REGS[CDTR] = data;
    HW_REGS[CCR] = 0x84;           /* start transfer as master */
    for (i = 0; i < timeout; i++) {
        if (!(HW_REGS[CCR] & 0x02)) {
            HW_REGS[CCR] = 0x80;           /* release port to idle */
            HW_REGS[CCSR] &= (u8)~0x02;    /* close channel */
            return LINK_OK;
        }
    }
    HW_REGS[CCR] = 0x80;           /* cancel / reset */
    HW_REGS[CCSR] &= (u8)~0x02;    /* close channel */
    return LINK_TIMEOUT;
}

u8 linkTryRecvByte(u8 *out, u16 timeout) {
    u16 i;
    HW_REGS[CCSR] |= 0x02;   /* open channel */
    for (i = 0; i < LINK_CHAN_OPEN_TIMEOUT; i++) {
        if (HW_REGS[CCSR] & 0x01) break;
    }
    HW_REGS[CDTR] = LINK_RECV_CDTR_DUMMY;  /* VUEngine: receiver writes before start */
    HW_REGS[CCR] = 0x94;           /* start transfer as remote */
    for (i = 0; i < timeout; i++) {
        if (!(HW_REGS[CCR] & 0x02)) {
            *out = HW_REGS[CDRR];
            HW_REGS[CCR] = 0x80;           /* release port to idle */
            HW_REGS[CCSR] &= (u8)~0x02;    /* close channel */
            return LINK_OK;
        }
    }
    HW_REGS[CCR] = 0x80;           /* cancel / reset */
    HW_REGS[CCSR] &= (u8)~0x02;    /* close channel */
    return LINK_TIMEOUT;
}

/* ---- Multi-byte ---- */

void linkSendPacket(const u8 *buf, u8 len) {
    u8 i;
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

void linkExchangeState(u8 *sendBuf, u8 *recvBuf) {
    if (g_isHost) {
        /* HOST sends first, then receives */
        linkSendPacket(sendBuf, 8);
        linkRecvPacket(recvBuf, 8);
    } else {
        /* JOIN receives first, then sends */
        linkRecvPacket(recvBuf, 8);
        linkSendPacket(sendBuf, 8);
    }
}
