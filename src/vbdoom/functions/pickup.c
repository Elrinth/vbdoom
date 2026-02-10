#include <libgccvb.h>
#include <mem.h>
#include "pickup.h"
#include "doomgfx.h"
#include "sndplay.h"

/* Global pickup array */
Pickup g_pickups[MAX_PICKUPS];

/* Flash state */
u8 g_flashTimer = 0;
u8 g_flashType = 0;
u8 g_pickedUpWeapon = 0;

void initPickups(void) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        g_pickups[i].active = false;
    }

    /* Place static pickups in the E1M1-inspired map.
     * Positions are in 8.8 fixed-point (tile * 256 + offset).
     */

    /* Small health kit in armor room (W), tile (4, 22) */
    g_pickups[0].x = 4 * 256 + 128;
    g_pickups[0].y = 22 * 256 + 128;
    g_pickups[0].type = PICKUP_HEALTH_SMALL;
    g_pickups[0].active = true;

    /* Ammo clip in supply room (E), tile (27, 22) */
    g_pickups[1].x = 27 * 256 + 128;
    g_pickups[1].y = 22 * 256 + 128;
    g_pickups[1].type = PICKUP_AMMO_CLIP;
    g_pickups[1].active = true;

    /* Shotgun pickup near start area, tile (15, 26) */
    g_pickups[2].x = 15 * 256 + 128;
    g_pickups[2].y = 26 * 256 + 128;
    g_pickups[2].type = PICKUP_WEAPON_SHOTGUN;
    g_pickups[2].active = true;
}

bool spawnPickup(u8 type, u16 x, u16 y) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        if (!g_pickups[i].active) {
            g_pickups[i].x = x;
            g_pickups[i].y = y;
            g_pickups[i].type = type;
            g_pickups[i].active = true;
            return true;
        }
    }
    return false; /* no free slot */
}

bool updatePickups(u16 playerX, u16 playerY, u8 *ammo, u8 *health) {
    u8 i;
    bool pickedUp = false;

    for (i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g_pickups[i];
        s16 dx, dy;

        if (!p->active) continue;

        /* AABB distance check (same style as enemy collision) */
        dx = (s16)playerX - (s16)p->x;
        dy = (s16)playerY - (s16)p->y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;

        if (dx < (s16)PICKUP_RADIUS && dy < (s16)PICKUP_RADIUS) {
            /* Player is touching this pickup -- apply effect */
            switch (p->type) {
            case PICKUP_AMMO_CLIP:
                if (*ammo < 200) {
                    *ammo += AMMO_CLIP_AMOUNT;
                    if (*ammo > 200) *ammo = 200;
                }
                break;
            case PICKUP_HEALTH_SMALL:
                if (*health < 100) {
                    *health += HEALTH_SMALL_AMOUNT;
                    if (*health > 100) *health = 100;
                } else {
                    continue; /* don't pick up if already full */
                }
                break;
            case PICKUP_HEALTH_LARGE:
                if (*health < 100) {
                    *health += HEALTH_LARGE_AMOUNT;
                    if (*health > 100) *health = 100;
                } else {
                    continue; /* don't pick up if already full */
                }
                break;
            case PICKUP_WEAPON_SHOTGUN:
                /* Weapon pickup: flag is set in g_pickedUpWeapon, gameLoop handles switch */
                g_pickedUpWeapon = 3;
                break;
            }

            p->active = false;
            pickedUp = true;

            /* Trigger pickup flash (2 frames) */
            g_flashTimer = 3;
            g_flashType = 0; /* 0 = pickup flash */

            /* Play pickup blip sound */
            playPickupSound();
        }
    }

    return pickedUp;
}
