/*
 * teleport.c -- Teleport fog visual effect system
 */

#include <libgccvb.h>
#include "teleport.h"

TeleportEffect g_teleportFX[MAX_TELEPORT_FX];

bool spawnTeleportFX(u16 x, u16 y) {
    u8 i;
    for (i = 0; i < MAX_TELEPORT_FX; i++) {
        if (!g_teleportFX[i].active) {
            g_teleportFX[i].x = x;
            g_teleportFX[i].y = y;
            g_teleportFX[i].frame = 0;
            g_teleportFX[i].timer = 0;
            g_teleportFX[i].active = true;
            return true;
        }
    }
    return false;  /* no free slot */
}

void updateTeleportFX(void) {
    u8 i;
    for (i = 0; i < MAX_TELEPORT_FX; i++) {
        TeleportEffect *fx = &g_teleportFX[i];
        if (!fx->active) continue;

        fx->timer++;
        if (fx->timer >= TELEPORT_ANIM_RATE) {
            fx->timer = 0;
            fx->frame++;
            if (fx->frame >= TELEPORT_FRAME_COUNT) {
                fx->active = false;  /* animation finished */
            }
        }
    }
}

TeleportEffect* getActiveTeleportFX(void) {
    u8 i;
    for (i = 0; i < MAX_TELEPORT_FX; i++) {
        if (g_teleportFX[i].active)
            return &g_teleportFX[i];
    }
    return (TeleportEffect*)0;
}
