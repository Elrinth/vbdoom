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

void initDoors(void) {
    u8 i;
    /* Scan g_map for door and switch tiles and register them */
    g_numDoors = 0;
    g_numSwitches = 0;
    g_levelComplete = 0;

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

/* Register a door at a map position */
void registerDoor(u8 tileX, u8 tileY) {
    if (g_numDoors >= MAX_DOORS) return;
    g_doors[g_numDoors].tileX = tileX;
    g_doors[g_numDoors].tileY = tileY;
    g_doors[g_numDoors].state = DOOR_CLOSED;
    g_doors[g_numDoors].openAmount = 0;
    g_doors[g_numDoors].timer = 0;
    g_doors[g_numDoors].originalMap = WALL_TYPE_DOOR;
    g_numDoors++;
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

static s8 findDoorAt(u8 tx, u8 ty) {
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

u8 playerActivate(u16 playerX, u16 playerY, s16 playerA) {
    /* Use raycaster center-column data from the previous frame.
     * g_centerWallType/TileX/TileY are set during TraceFrame() for x==192.
     * g_wallSso[24] is the wall half-height at center column (higher = closer). */
    extern u8 g_wallSso[];
    u8 wt = g_centerWallType;
    s8 idx;

    if (wt == 0) return 0;            /* looking at empty space */
    if (g_wallSso[24] < 25) return 0; /* wall too far away */

    if (wt == WALL_TYPE_DOOR) {
        idx = findDoorAt(g_centerWallTileX, g_centerWallTileY);
        if (idx >= 0) { toggleDoor((u8)idx); return 1; }
    }
    if (wt == WALL_TYPE_SWITCH) {
        idx = findSwitchAt(g_centerWallTileX, g_centerWallTileY);
        if (idx >= 0) { activateSwitch((u8)idx); return 1; }
    }

    return 2; /* non-interactive wall -- play umf */
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
