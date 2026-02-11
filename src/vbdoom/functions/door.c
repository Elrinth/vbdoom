#include <libgccvb.h>
#include <mem.h>
#include "door.h"
#include "RayCasterFixed.h"
#include "RayCaster.h"
#include "sndplay.h"
#include "../assets/audio/doom_sfx.h"
#include "../assets/images/wall_textures.h"

/* g_map is defined in RayCasterData.h (included by RayCasterFixed.c) */
extern u8 g_map[];

/* Player position (defined in gameLoop.c) -- used to block door closing */
extern u16 fPlayerX, fPlayerY;

DoorState g_doors[MAX_DOORS];
SwitchState g_switches[MAX_SWITCHES];
u8 g_numDoors = 0;
u8 g_numSwitches = 0;
u8 g_levelComplete = 0;
u8 g_hasKeyRed = 0;
u8 g_hasKeyYellow = 0;
u8 g_hasKeyBlue = 0;

void initDoors(void) {
    u8 i;
    g_numDoors = 0;
    g_numSwitches = 0;
    g_levelComplete = 0;
    g_hasKeyRed = 0;
    g_hasKeyYellow = 0;
    g_hasKeyBlue = 0;

    for (i = 0; i < MAX_DOORS; i++) {
        g_doors[i].state = DOOR_CLOSED;
        g_doors[i].openAmount = 0;
        g_doors[i].timer = 0;
    }
    for (i = 0; i < MAX_SWITCHES; i++) {
        g_switches[i].activated = 0;
    }

    /* Doors and switches are registered manually in the level setup (Phase 7).
     * This just initializes the arrays. */
}

/* Register a door at a map position (tile 4=door, 6/7/8=secret, 9/10/11=key door) */
void registerDoor(u8 tileX, u8 tileY) {
    if (g_numDoors >= MAX_DOORS) return;
    {
        u8 orig = g_map[(u16)tileY * MAP_X + (u16)tileX];
        if (orig != WALL_TYPE_DOOR && orig != 6 && orig != 7 && orig != 8 && orig != 9 && orig != 10 && orig != 11)
            orig = WALL_TYPE_DOOR;
        g_doors[g_numDoors].tileX = tileX;
        g_doors[g_numDoors].tileY = tileY;
        g_doors[g_numDoors].state = DOOR_CLOSED;
        g_doors[g_numDoors].openAmount = 0;
        g_doors[g_numDoors].timer = 0;
        g_doors[g_numDoors].originalMap = orig;
        g_numDoors++;
    }
}

/* Register a switch at a map position */
void registerSwitch(u8 tileX, u8 tileY, u8 type, u8 linkedDoor) {
    if (g_numSwitches >= MAX_SWITCHES) return;
    g_switches[g_numSwitches].tileX = tileX;
    g_switches[g_numSwitches].tileY = tileY;
    g_switches[g_numSwitches].type = type;
    g_switches[g_numSwitches].linkedDoor = linkedDoor;
    g_switches[g_numSwitches].activated = 0;
    g_numSwitches++;
}

void updateDoors(void) {
    u8 i;
    for (i = 0; i < g_numDoors; i++) {
        DoorState *d = &g_doors[i];

        switch (d->state) {
        case DOOR_OPENING:
            d->openAmount += DOOR_OPEN_SPEED;
            if (d->openAmount >= DOOR_OPEN_MAX) {
                d->openAmount = DOOR_OPEN_MAX;
                d->state = DOOR_OPEN;
                d->timer = DOOR_STAY_TIME;
                /* Make tile passable */
                g_map[(u16)d->tileY * MAP_X + (u16)d->tileX] = 0;
            }
            break;

        case DOOR_OPEN:
            if (d->timer > 0) {
                d->timer--;
            } else {
                /* Don't close if the player is standing in the doorway */
                u8 ptx = (u8)(fPlayerX >> 8);
                u8 pty = (u8)(fPlayerY >> 8);
                if (ptx == d->tileX && pty == d->tileY) {
                    d->timer = DOOR_STAY_TIME; /* keep waiting */
                } else {
                    d->state = DOOR_CLOSING;
                    /* Restore wall tile NOW so raycaster renders during close animation */
                    g_map[(u16)d->tileY * MAP_X + (u16)d->tileX] = d->originalMap;
                    playPlayerSFX(SFX_DOOR_CLOSE);
                }
            }
            break;

        case DOOR_CLOSING:
            if (d->openAmount > DOOR_OPEN_SPEED)
                d->openAmount -= DOOR_OPEN_SPEED;
            else
                d->openAmount = 0;
            if (d->openAmount == 0) {
                d->state = DOOR_CLOSED;
            }
            break;
        }
    }
}

s8 findDoorAt(u8 tx, u8 ty) {
    u8 i;
    for (i = 0; i < g_numDoors; i++) {
        if (g_doors[i].tileX == tx && g_doors[i].tileY == ty)
            return (s8)i;
    }
    return -1;
}

static s8 findSwitchAt(u8 tx, u8 ty) {
    u8 i;
    for (i = 0; i < g_numSwitches; i++) {
        if (g_switches[i].tileX == tx && g_switches[i].tileY == ty)
            return (s8)i;
    }
    return -1;
}

/* Toggle a door: open if closed/closing, close if opening/open (like Doom) */
static void toggleDoor(u8 doorIdx) {
    DoorState *d = &g_doors[doorIdx];
    switch (d->state) {
    case DOOR_CLOSED:
    case DOOR_CLOSING:
        d->state = DOOR_OPENING;
        /* Map tile is already solid in both states -- keep it */
        playPlayerSFX(SFX_DOOR_OPEN);
        break;
    case DOOR_OPENING:
        d->state = DOOR_CLOSING;
        /* Map tile is already solid during OPENING -- keep it */
        playPlayerSFX(SFX_DOOR_CLOSE);
        break;
    case DOOR_OPEN:
        d->state = DOOR_CLOSING;
        /* Restore solid tile so raycaster renders during close animation */
        g_map[(u16)d->tileY * MAP_X + (u16)d->tileX] = d->originalMap;
        playPlayerSFX(SFX_DOOR_CLOSE);
        break;
    }
}

/* Activate a switch */
static void activateSwitch(u8 switchIdx) {
    SwitchState *s = &g_switches[switchIdx];
    if (s->activated) return;

    s->activated = 1;
    playPlayerSFX(SFX_SWITCH_ON);

    /* Swap switch texture in VRAM (copy "on" tiles over "off" tiles) */
    {
        u32 switchVramAddr = 0x00078000 + (u32)(WALL_TEX_CHAR_START + SWITCH_VRAM_OFFSET) * 16;
        copymem((void*)switchVramAddr, (void*)switchOnTiles, SWITCH_ON_TILES_BYTES);
    }

    if (s->type == SW_DOOR) {
        /* Open the linked door */
        if (s->linkedDoor < g_numDoors) {
            toggleDoor(s->linkedDoor);
        }
    } else if (s->type == SW_EXIT) {
        /* Level complete! */
        g_levelComplete = 1;
    }
}

u8 playerActivate(u16 playerX, u16 playerY, s16 playerA, u8 levelNum) {
    extern u8 g_wallSso[];
    u8 wt = g_centerWallType;
    s8 idx;
    u8 px = (u8)(playerX >> 8);
    u8 py = (u8)(playerY >> 8);

    /* Run door-from-position check first so E1M4 spawn door (27,28) always works when standing beside it,
     * regardless of center ray or view angle. */
    {
        /* E1M4 spawn exit: guarantee door (27,28) opens when player is on door or any adjacent tile */
        if (levelNum == 4) {
            if ((px == 27 && py == 28) || (px == 28 && py == 28) || (px == 29 && py == 28) ||
                (px == 26 && py == 28) || (px == 27 && py == 27) || (px == 27 && py == 29) ||
                (px == 30 && py == 28) || (px == 30 && py == 29) || (px == 29 && py == 29) || (px == 28 && py == 29)) {
                idx = findDoorAt(27, 28);
                if (idx >= 0 && (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING)) {
                    u8 cell = g_map[(u16)28 * MAP_X + (u16)27];
                    if (cell == 9 && !g_hasKeyRed)   { (void)0; }
                    else if (cell == 10 && !g_hasKeyYellow) { (void)0; }
                    else if (cell == 11 && !g_hasKeyBlue) { (void)0; }
                    else { toggleDoor((u8)idx); return 1; }
                }
            }
        }
        s8 fdx = 0, fdy = 0;
        s16 fa = playerA & 1023;
        if (fa < 128 || fa >= 896) fdy = 1;
        else if (fa >= 128 && fa < 384) fdx = 1;
        else if (fa >= 384 && fa < 640) fdy = -1;
        else fdx = -1;
        /* Tile in front */
        {
            u8 checkX = (u8)((s16)px + fdx);
            u8 checkY = (u8)((s16)py + fdy);
            idx = findDoorAt(checkX, checkY);
            if (idx >= 0) {
                u8 cell = g_map[(u16)checkY * MAP_X + (u16)checkX];
                if (cell == 9 && !g_hasKeyRed)   return 2;
                if (cell == 10 && !g_hasKeyYellow) return 2;
                if (cell == 11 && !g_hasKeyBlue) return 2;
                if (g_doors[(u8)idx].state == DOOR_OPEN) { toggleDoor((u8)idx); return 1; }
                if (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING) {
                    toggleDoor((u8)idx); return 1;
                }
            }
        }
        /* Two steps in front (e.g. door at (27,28) when at (29,28) facing west) */
        {
            s16 cx2 = (s16)px + 2 * fdx;
            s16 cy2 = (s16)py + 2 * fdy;
            if (cx2 >= 0 && cx2 < (s16)MAP_X && cy2 >= 0 && cy2 < (s16)MAP_Y) {
                u8 checkX2 = (u8)cx2;
                u8 checkY2 = (u8)cy2;
                idx = findDoorAt(checkX2, checkY2);
                if (idx >= 0 && (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING)) {
                    u8 cell = g_map[(u16)checkY2 * MAP_X + (u16)checkX2];
                    if (cell == 9 && !g_hasKeyRed)   { (void)0; }
                    else if (cell == 10 && !g_hasKeyYellow) { (void)0; }
                    else if (cell == 11 && !g_hasKeyBlue) { (void)0; }
                    else { toggleDoor((u8)idx); return 1; }
                }
            }
        }
        /* One step left and one step right of player (perpendicular to facing) */
        {
            s8 ldx = 0, ldy = 0;
            if (fdx == 0 && fdy == 1)  { ldx = -1; ldy = 0; }  /* facing N -> left W, right E */
            else if (fdx == 0 && fdy == -1) { ldx = 1; ldy = 0; }
            else if (fdx == 1 && fdy == 0)  { ldx = 0; ldy = 1; }  /* facing E -> left N, right S */
            else if (fdx == -1 && fdy == 0) { ldx = 0; ldy = -1; }
            if (ldx != 0 || ldy != 0) {
                s16 lx = (s16)px + ldx, ly = (s16)py + ldy;
                s16 rx = (s16)px - ldx, ry = (s16)py - ldy;
                if (lx >= 0 && lx < (s16)MAP_X && ly >= 0 && ly < (s16)MAP_Y) {
                    idx = findDoorAt((u8)lx, (u8)ly);
                    if (idx >= 0 && (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING)) {
                        u8 cell = g_map[(u16)(u8)ly * MAP_X + (u16)(u8)lx];
                        if (cell != 9 && cell != 10 && cell != 11) { toggleDoor((u8)idx); return 1; }
                        if (cell == 9 && g_hasKeyRed)   { toggleDoor((u8)idx); return 1; }
                        if (cell == 10 && g_hasKeyYellow) { toggleDoor((u8)idx); return 1; }
                        if (cell == 11 && g_hasKeyBlue) { toggleDoor((u8)idx); return 1; }
                    }
                }
                if (rx >= 0 && rx < (s16)MAP_X && ry >= 0 && ry < (s16)MAP_Y) {
                    idx = findDoorAt((u8)rx, (u8)ry);
                    if (idx >= 0 && (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING)) {
                        u8 cell = g_map[(u16)(u8)ry * MAP_X + (u16)(u8)rx];
                        if (cell != 9 && cell != 10 && cell != 11) { toggleDoor((u8)idx); return 1; }
                        if (cell == 9 && g_hasKeyRed)   { toggleDoor((u8)idx); return 1; }
                        if (cell == 10 && g_hasKeyYellow) { toggleDoor((u8)idx); return 1; }
                        if (cell == 11 && g_hasKeyBlue) { toggleDoor((u8)idx); return 1; }
                    }
                }
            }
        }
        /* All 4 adjacent tiles */
        {
            const s8 dx[] = { 0, 1, 0, -1 };
            const s8 dy[] = { 1, 0, -1, 0 };
            int k;
            for (k = 0; k < 4; k++) {
                u8 ax = (u8)((s16)px + dx[k]);
                u8 ay = (u8)((s16)py + dy[k]);
                idx = findDoorAt(ax, ay);
                if (idx >= 0 && (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING)) {
                    u8 cell = g_map[(u16)ay * MAP_X + (u16)ax];
                    if (cell == 9 && !g_hasKeyRed)   continue;
                    if (cell == 10 && !g_hasKeyYellow) continue;
                    if (cell == 11 && !g_hasKeyBlue) continue;
                    toggleDoor((u8)idx);
                    return 1;
                }
            }
        }
        /* 8-neighbor fallback (cardinals + diagonals) so door opens regardless of facing */
        {
            const s8 dx8[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
            const s8 dy8[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
            int k;
            for (k = 0; k < 8; k++) {
                s16 ax = (s16)px + dx8[k];
                s16 ay = (s16)py + dy8[k];
                if (ax < 0 || ax >= (s16)MAP_X || ay < 0 || ay >= (s16)MAP_Y) continue;
                idx = findDoorAt((u8)ax, (u8)ay);
                if (idx >= 0 && (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING)) {
                    u8 cell = g_map[(u16)(u8)ay * MAP_X + (u16)(u8)ax];
                    if (cell == 9 && !g_hasKeyRed)   continue;
                    if (cell == 10 && !g_hasKeyYellow) continue;
                    if (cell == 11 && !g_hasKeyBlue) continue;
                    toggleDoor((u8)idx);
                    return 1;
                }
            }
        }
        /* Player's own tile (e.g. right on boundary so (playerX>>8) rounds into door cell) */
        idx = findDoorAt(px, py);
        if (idx >= 0 && (g_doors[(u8)idx].state == DOOR_CLOSED || g_doors[(u8)idx].state == DOOR_CLOSING)) {
            u8 cell = g_map[(u16)py * MAP_X + (u16)px];
            if (cell != 9 && cell != 10 && cell != 11) { toggleDoor((u8)idx); return 1; }
            if (cell == 9 && g_hasKeyRed)   { toggleDoor((u8)idx); return 1; }
            if (cell == 10 && g_hasKeyYellow) { toggleDoor((u8)idx); return 1; }
            if (cell == 11 && g_hasKeyBlue) { toggleDoor((u8)idx); return 1; }
        }
    }

    /* Center ray: door or switch (for when not standing adjacent) */
    if (wt == WALL_TYPE_DOOR || (wt >= 6 && wt <= 11)) {
        if (wt == 9 && !g_hasKeyRed)   return 2;
        if (wt == 10 && !g_hasKeyYellow) return 2;
        if (wt == 11 && !g_hasKeyBlue)  return 2;
        idx = findDoorAt(g_centerWallTileX, g_centerWallTileY);
        if (idx >= 0) { toggleDoor((u8)idx); return 1; }
    }
    if (wt == WALL_TYPE_SWITCH) {
        if (g_wallSso[RAYCAST_CENTER_COL] < 25) return 0;
        idx = findSwitchAt(g_centerWallTileX, g_centerWallTileY);
        if (idx >= 0) { activateSwitch((u8)idx); return 1; }
    }

    if (g_wallSso[RAYCAST_CENTER_COL] < 25) return 0;
    return 2;
}

u8 getDoorOpenAmount(u8 tileX, u8 tileY) {
    u8 i;
    for (i = 0; i < g_numDoors; i++) {
        if (g_doors[i].tileX == tileX && g_doors[i].tileY == tileY) {
            return g_doors[i].openAmount;
        }
    }
    return 0;
}
