#ifndef _FUNCTIONS_LINK_H
#define _FUNCTIONS_LINK_H

#include <types.h>
#include <stdbool.h>

/*
 * Virtual Boy Link Cable (EXT Port) communication.
 *
 * Protocol: 1 byte per transfer.  One side is "master" (drives clock, CCR=0x84)
 * and the other is "remote" (receives clock, CCR=0x94).  Poll CCR bit 1 (&2)
 * until clear to know transfer is complete.
 *
 * Based on reference implementations from VB Tic-Tac-Toe and 3D BattleSnake.
 */

/* Game modes */
#define GAMEMODE_COOP       0
#define GAMEMODE_DEATHMATCH 1

/* Handshake bytes */
#define LINK_JOIN_REQ   0xAA
#define LINK_HOST_ACK   0x55
#define LINK_SYNC_OK      0xCC
#define LINK_JOINER_READY 0xDD  /* sent by JOINER when on wait screen */
#define LINK_GAME_START   0xBB  /* sent by HOST before level/mode data */

/* Transfer result codes */
#define LINK_OK         0
#define LINK_TIMEOUT    1

/* ---- Globals ---- */
extern bool g_isMultiplayer;   /* true when a multiplayer session is active */
extern bool g_isHost;          /* true = HOST, false = JOIN */
extern u8   g_gameMode;        /* GAMEMODE_COOP or GAMEMODE_DEATHMATCH */

/* ---- Player 2 state (received from remote each frame) ---- */
extern u16  g_player2X;        /* 8.8 fixed-point */
extern u16  g_player2Y;
extern u16  g_player2Angle;    /* 0-1023 */
extern u8   g_player2Flags;    /* bit 0=shooting, bits 1-3=weapon, bit 4=took damage, bit 5=died */
extern u8   g_player2Health;
extern bool g_player2Alive;

/* Flag bits within g_player2Flags */
#define P2F_SHOOTING    0x01
#define P2F_WEAPON_MASK 0x0E   /* bits 1-3 */
#define P2F_WEAPON_SHIFT 1
#define P2F_TOOK_DAMAGE  0x10
#define P2F_DIED         0x20
#define P2F_FIRED_ROCKET 0x40   /* set on the frame P2 fires a rocket */

/* ---- Frag tracking ---- */
extern u8   g_fragCount;       /* kills by local player */
extern u8   g_deathCount;      /* times local player died */

/* ---- Player 2 rendering constants ---- */
/* Player 2 uses enemy slot 4 (world 30, BGMap 7, chars 1239-1302).
 * When multiplayer is active, max visible enemies drops from 5 to 4. */
#define P2_RENDER_SLOT      4
#define P2_WORLD_NUM        30   /* world 30 (enemy slot 4) */
#define P2_BGMAP_IDX        7    /* BGMap(7) = ENEMY_BGMAP_START + 4 */
#define P2_CHAR_START       1239 /* ZOMBIE_CHAR_START + 4*64 */

/* Player 2 spawn points (set per level) */
extern u16  g_p2SpawnX;
extern u16  g_p2SpawnY;
extern u16  g_p2SpawnAngle;

/* Player 2 animation state (for walk animation) */
extern u8   g_p2AnimFrame;
extern u8   g_p2AnimTimer;
extern u8   g_p2LastFrame;     /* last loaded Marine frame (skip if same) */

/* ---- API ---- */

/* Initialize link cable hardware registers. */
void linkInit(void);

/* Blocking send: write 1 byte as master. */
void linkSendByte(u8 data);

/* Blocking receive: read 1 byte as remote. Returns received byte. */
u8   linkRecvByte(void);

/* Try to send 1 byte with a loop-count timeout.
 * Returns LINK_OK on success, LINK_TIMEOUT if the transfer didn't complete. */
u8   linkTrySendByte(u8 data, u16 timeout);

/* Try to receive 1 byte with a loop-count timeout.
 * On success, stores received byte in *out and returns LINK_OK.
 * On timeout, returns LINK_TIMEOUT. */
u8   linkTryRecvByte(u8 *out, u16 timeout);

/* Send a multi-byte packet as master (blocking). */
void linkSendPacket(const u8 *buf, u8 len);

/* Receive a multi-byte packet as remote (blocking). */
void linkRecvPacket(u8 *buf, u8 len);

/* High-level: exchange 8-byte state packets.
 * HOST sends first then receives; JOIN receives first then sends.
 * sendBuf and recvBuf must both be 8 bytes. */
void linkExchangeState(u8 *sendBuf, u8 *recvBuf);

#endif
