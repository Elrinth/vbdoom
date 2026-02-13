/*
 * teleport.h -- Teleport fog visual effect system
 *
 * Used for:
 *   - Pickup respawn animation in deathmatch
 *   - Player respawn animation in deathmatch
 *
 * Rendered using enemy slot 0 (world 26, BGMap 3, chars 983-1046)
 * which is free in deathmatch mode (no enemies).
 */
#ifndef _FUNCTIONS_TELEPORT_H
#define _FUNCTIONS_TELEPORT_H

#include <types.h>
#include <stdbool.h>

typedef struct {
    u16  x;        /* world position 8.8 fixed-point */
    u16  y;
    u8   frame;    /* current animation frame (0-9) */
    u8   timer;    /* frame-rate sub-counter */
    bool active;
} TeleportEffect;

#define MAX_TELEPORT_FX      2
#define TELEPORT_ANIM_RATE   2   /* advance sprite frame every 2 game frames */
#define TELEPORT_FRAME_COUNT 10

extern TeleportEffect g_teleportFX[MAX_TELEPORT_FX];

/* Spawn a new teleport effect at the given world position.
 * Returns true if a slot was available. */
bool spawnTeleportFX(u16 x, u16 y);

/* Advance animation timers. Call once per frame. */
void updateTeleportFX(void);

/* Get the first active teleport effect (for rendering).
 * Returns NULL if none active. */
TeleportEffect* getActiveTeleportFX(void);

#endif
