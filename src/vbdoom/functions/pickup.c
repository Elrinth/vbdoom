#include <libgccvb.h>
#include <mem.h>
#include "pickup.h"
#include "door.h"
#include "doomgfx.h"
#include "sndplay.h"
#include "link.h"
#include "teleport.h"
#include "../assets/audio/doom_sfx.h"

/* Respawn time in frames (~5 seconds at 20fps) */
#define PICKUP_RESPAWN_TIME 100

/* When respawnTimer reaches this value, start the teleport animation.
 * 10 frames * 2 rate = 20 game frames of animation before pickup appears. */
#define PICKUP_TELEPORT_START (TELEPORT_FRAME_COUNT * TELEPORT_ANIM_RATE)

/* Global pickup array */
Pickup g_pickups[MAX_PICKUPS];

/* Flash state */
u8 g_flashTimer = 0;
u8 g_flashType = 0;
u8 g_pickedUpWeapon = 0;

/* Item tracking (for level stats screen) */
u8 g_itemsCollected = 0;
u8 g_totalItems = 0;

/* Animation rate for ping-pong pickups */
#define PICKUP_ANIM_RATE  5   /* frames per animation step (was 10) */

void initPickups(void) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        g_pickups[i].active = false;
        g_pickups[i].animFrame = 0;
        g_pickups[i].animTimer = 0;
    }

    /* Place static pickups on the map.
     * Positions are in 8.8 fixed-point (tile * 256 + offset).
     */

    /* 0: Small health kit, west side room (4, 22) */
    g_pickups[0].x = 4 * 256 + 128;
    g_pickups[0].y = 22 * 256 + 128;
    g_pickups[0].type = PICKUP_HEALTH_SMALL;
    g_pickups[0].active = true;

    /* 1: Ammo clip, east side room (27, 22) */
    g_pickups[1].x = 27 * 256 + 128;
    g_pickups[1].y = 22 * 256 + 128;
    g_pickups[1].type = PICKUP_AMMO_CLIP;
    g_pickups[1].active = true;

    /* 2: Shotgun pickup, main hall (15, 24) */
    g_pickups[2].x = 15 * 256 + 128;
    g_pickups[2].y = 24 * 256 + 128;
    g_pickups[2].type = PICKUP_WEAPON_SHOTGUN;
    g_pickups[2].active = true;

    /* 3: Large health kit, zigzag room (3, 16) */
    g_pickups[3].x = 3 * 256 + 128;
    g_pickups[3].y = 16 * 256 + 128;
    g_pickups[3].type = PICKUP_HEALTH_LARGE;
    g_pickups[3].active = true;

    /* 4: Ammo clip, zigzag room east (20, 14) */
    g_pickups[4].x = 20 * 256 + 128;
    g_pickups[4].y = 14 * 256 + 128;
    g_pickups[4].type = PICKUP_AMMO_CLIP;
    g_pickups[4].active = true;

    /* 5: Shells box, east side room (26, 14) */
    g_pickups[5].x = 26 * 256 + 128;
    g_pickups[5].y = 14 * 256 + 128;
    g_pickups[5].type = PICKUP_SHELLS;
    g_pickups[5].active = true;

    /* 6: Helmet pickup, computer room (16, 6) */
    g_pickups[6].x = 16 * 256 + 128;
    g_pickups[6].y = 6 * 256 + 128;
    g_pickups[6].type = PICKUP_HELMET;
    g_pickups[6].active = true;

    /* 7: Armor pickup (rare), armor room behind locked door (4, 9) */
    g_pickups[7].x = 4 * 256 + 128;
    g_pickups[7].y = 9 * 256 + 128;
    g_pickups[7].type = PICKUP_ARMOR;
    g_pickups[7].active = true;

    /* 8: Small health kit, main hall west (2, 21) */
    g_pickups[8].x = 2 * 256 + 128;
    g_pickups[8].y = 21 * 256 + 128;
    g_pickups[8].type = PICKUP_HEALTH_SMALL;
    g_pickups[8].active = true;

    /* 9: Ammo clip, start area (13, 28) */
    g_pickups[9].x = 13 * 256 + 128;
    g_pickups[9].y = 28 * 256 + 128;
    g_pickups[9].type = PICKUP_AMMO_CLIP;
    g_pickups[9].active = true;

    /* 10: Small health kit, computer room north (14, 5) */
    g_pickups[10].x = 14 * 256 + 128;
    g_pickups[10].y = 5 * 256 + 128;
    g_pickups[10].type = PICKUP_HEALTH_SMALL;
    g_pickups[10].active = true;

    /* 11: Shells box, zigzag room (7, 18) */
    g_pickups[11].x = 7 * 256 + 128;
    g_pickups[11].y = 18 * 256 + 128;
    g_pickups[11].type = PICKUP_SHELLS;
    g_pickups[11].active = true;

    /* 12: Helmet pickup, east corridor (26, 13) */
    g_pickups[12].x = 26 * 256 + 128;
    g_pickups[12].y = 13 * 256 + 128;
    g_pickups[12].type = PICKUP_HELMET;
    g_pickups[12].active = true;

    /* 13: Rocket launcher pickup, computer room south (20, 8) */
    g_pickups[13].x = 20 * 256 + 128;
    g_pickups[13].y = 8 * 256 + 128;
    g_pickups[13].type = PICKUP_WEAPON_ROCKET;
    g_pickups[13].active = true;

    /* 14: Chaingun pickup, east corridor (29, 7) */
    g_pickups[14].x = 29 * 256 + 128;
    g_pickups[14].y = 7 * 256 + 128;
    g_pickups[14].type = PICKUP_WEAPON_CHAINGUN;
    g_pickups[14].active = true;
}

void initPickupsE1M2(void) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        g_pickups[i].active = false;
        g_pickups[i].animFrame = 0;
        g_pickups[i].animTimer = 0;
    }

    /* E1M2: Wolfenstein bunker pickups */

    /* 0: Ammo clip, south corridor west alcove (5, 24) */
    g_pickups[0].x = 5 * 256 + 128;
    g_pickups[0].y = 24 * 256 + 128;
    g_pickups[0].type = PICKUP_AMMO_CLIP;
    g_pickups[0].active = true;

    /* 1: Small health, south corridor east alcove (26, 24) */
    g_pickups[1].x = 26 * 256 + 128;
    g_pickups[1].y = 24 * 256 + 128;
    g_pickups[1].type = PICKUP_HEALTH_SMALL;
    g_pickups[1].active = true;

    /* 2: Large health, central command room (15, 11) */
    g_pickups[2].x = 15 * 256 + 128;
    g_pickups[2].y = 11 * 256 + 128;
    g_pickups[2].type = PICKUP_HEALTH_LARGE;
    g_pickups[2].active = true;

    /* 3: Shells box, east armory (29, 15) */
    g_pickups[3].x = 29 * 256 + 128;
    g_pickups[3].y = 15 * 256 + 128;
    g_pickups[3].type = PICKUP_SHELLS;
    g_pickups[3].active = true;

    /* 4: Ammo clip, west barracks (3, 17) */
    g_pickups[4].x = 3 * 256 + 128;
    g_pickups[4].y = 17 * 256 + 128;
    g_pickups[4].type = PICKUP_AMMO_CLIP;
    g_pickups[4].active = true;

    /* 5: Shells box, west barracks near door (3, 14) */
    g_pickups[5].x = 3 * 256 + 128;
    g_pickups[5].y = 14 * 256 + 128;
    g_pickups[5].type = PICKUP_SHELLS;
    g_pickups[5].active = true;

    /* 6: Small health, north corridor (10, 9) */
    g_pickups[6].x = 10 * 256 + 128;
    g_pickups[6].y = 9 * 256 + 128;
    g_pickups[6].type = PICKUP_HEALTH_SMALL;
    g_pickups[6].active = true;

    /* 7: Helmet, officer quarters NW (8, 5) */
    g_pickups[7].x = 8 * 256 + 128;
    g_pickups[7].y = 5 * 256 + 128;
    g_pickups[7].type = PICKUP_HELMET;
    g_pickups[7].active = true;

    /* 8: Ammo clip, central room west (7, 11) */
    g_pickups[8].x = 7 * 256 + 128;
    g_pickups[8].y = 11 * 256 + 128;
    g_pickups[8].type = PICKUP_AMMO_CLIP;
    g_pickups[8].active = true;

    /* 9: Small health, start room (13, 28) */
    g_pickups[9].x = 13 * 256 + 128;
    g_pickups[9].y = 28 * 256 + 128;
    g_pickups[9].type = PICKUP_HEALTH_SMALL;
    g_pickups[9].active = true;

    /* 10: Armor, behind locked weapon closet (3, 16) */
    g_pickups[10].x = 3 * 256 + 128;
    g_pickups[10].y = 16 * 256 + 128;
    g_pickups[10].type = PICKUP_ARMOR;
    g_pickups[10].active = true;

    /* 11: Ammo clip, exit room (25, 3) */
    g_pickups[11].x = 25 * 256 + 128;
    g_pickups[11].y = 3 * 256 + 128;
    g_pickups[11].type = PICKUP_AMMO_CLIP;
    g_pickups[11].active = true;
}

void initPickupsE1M3(void) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        g_pickups[i].active = false;
        g_pickups[i].animFrame = 0;
        g_pickups[i].animTimer = 0;
    }

    /* E1M3: Hell-touched research facility pickups */

    /* 0: Shotgun, south corridor (accessible early) (10, 23) */
    g_pickups[0].x = 10 * 256 + 128;
    g_pickups[0].y = 23 * 256 + 128;
    g_pickups[0].type = PICKUP_WEAPON_SHOTGUN;
    g_pickups[0].active = true;

    /* 1: Ammo clip, start room (13, 28) */
    g_pickups[1].x = 13 * 256 + 128;
    g_pickups[1].y = 28 * 256 + 128;
    g_pickups[1].type = PICKUP_AMMO_CLIP;
    g_pickups[1].active = true;

    /* 2: Shells box, south corridor east (22, 24) */
    g_pickups[2].x = 22 * 256 + 128;
    g_pickups[2].y = 24 * 256 + 128;
    g_pickups[2].type = PICKUP_SHELLS;
    g_pickups[2].active = true;

    /* 3: Small health, west wing storage (3, 19) */
    g_pickups[3].x = 3 * 256 + 128;
    g_pickups[3].y = 19 * 256 + 128;
    g_pickups[3].type = PICKUP_HEALTH_SMALL;
    g_pickups[3].active = true;

    /* 4: Large health, west wing (4, 16) */
    g_pickups[4].x = 4 * 256 + 128;
    g_pickups[4].y = 16 * 256 + 128;
    g_pickups[4].type = PICKUP_HEALTH_LARGE;
    g_pickups[4].active = true;

    /* 5: Shells box, east wing lab (29, 17) */
    g_pickups[5].x = 29 * 256 + 128;
    g_pickups[5].y = 17 * 256 + 128;
    g_pickups[5].type = PICKUP_SHELLS;
    g_pickups[5].active = true;

    /* 6: Ammo clip, east wing entrance (27, 15) */
    g_pickups[6].x = 27 * 256 + 128;
    g_pickups[6].y = 15 * 256 + 128;
    g_pickups[6].type = PICKUP_AMMO_CLIP;
    g_pickups[6].active = true;

    /* 7: Small health, central arena west (2, 9) */
    g_pickups[7].x = 2 * 256 + 128;
    g_pickups[7].y = 9 * 256 + 128;
    g_pickups[7].type = PICKUP_HEALTH_SMALL;
    g_pickups[7].active = true;

    /* 8: Helmet, central arena east (25, 9) */
    g_pickups[8].x = 25 * 256 + 128;
    g_pickups[8].y = 9 * 256 + 128;
    g_pickups[8].type = PICKUP_HELMET;
    g_pickups[8].active = true;

    /* 9: Armor, hidden armory (secret 1) (2, 4) */
    g_pickups[9].x = 2 * 256 + 128;
    g_pickups[9].y = 4 * 256 + 128;
    g_pickups[9].type = PICKUP_ARMOR;
    g_pickups[9].active = true;

    /* 10: Rocket launcher, locked tech room (secret 2) (29, 3) */
    g_pickups[10].x = 29 * 256 + 128;
    g_pickups[10].y = 3 * 256 + 128;
    g_pickups[10].type = PICKUP_WEAPON_ROCKET;
    g_pickups[10].active = true;

    /* 11: Large health, command center near exit (14, 2) */
    g_pickups[11].x = 14 * 256 + 128;
    g_pickups[11].y = 2 * 256 + 128;
    g_pickups[11].type = PICKUP_HEALTH_LARGE;
    g_pickups[11].active = true;
}

void initPickupsE1M4(void) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        g_pickups[i].active = false;
        g_pickups[i].animFrame = 0;
        g_pickups[i].animTimer = 0;
    }

    /* E1M4: web-editor export */
    g_pickups[0].x = 14 * 256 + 128;
    g_pickups[0].y = 27 * 256 + 128;
    g_pickups[0].type = PICKUP_SHELLS;
    g_pickups[0].active = true;
    g_pickups[1].x = 15 * 256 + 128;
    g_pickups[1].y = 27 * 256 + 128;
    g_pickups[1].type = PICKUP_SHELLS;
    g_pickups[1].active = true;
    g_pickups[2].x = 16 * 256 + 128;
    g_pickups[2].y = 27 * 256 + 128;
    g_pickups[2].type = PICKUP_HEALTH_SMALL;
    g_pickups[2].active = true;
    g_pickups[3].x = 14 * 256 + 128;
    g_pickups[3].y = 19 * 256 + 128;
    g_pickups[3].type = PICKUP_WEAPON_ROCKET;
    g_pickups[3].active = true;
    g_pickups[4].x = 17 * 256 + 128;
    g_pickups[4].y = 17 * 256 + 128;
    g_pickups[4].type = PICKUP_HELMET;
    g_pickups[4].active = true;
    g_pickups[5].x = 20 * 256 + 128;
    g_pickups[5].y = 19 * 256 + 128;
    g_pickups[5].type = PICKUP_ARMOR;
    g_pickups[5].active = true;
    g_pickups[6].x = 16 * 256 + 128;
    g_pickups[6].y = 23 * 256 + 128;
    g_pickups[6].type = PICKUP_WEAPON_SHOTGUN;
    g_pickups[6].active = true;
    g_pickups[7].x = 2 * 256 + 128;
    g_pickups[7].y = 21 * 256 + 128;
    g_pickups[7].type = PICKUP_HEALTH_LARGE;
    g_pickups[7].active = true;
    g_pickups[8].x = 2 * 256 + 128;
    g_pickups[8].y = 5 * 256 + 128;
    g_pickups[8].type = PICKUP_HELMET;
    g_pickups[8].active = true;
    g_pickups[9].x = 3 * 256 + 128;
    g_pickups[9].y = 5 * 256 + 128;
    g_pickups[9].type = PICKUP_SHELLS;
    g_pickups[9].active = true;
    g_pickups[10].x = 13 * 256 + 128;
    g_pickups[10].y = 13 * 256 + 128;
    g_pickups[10].type = PICKUP_HEALTH_SMALL;
    g_pickups[10].active = true;
    g_pickups[11].x = 1 * 256 + 128;
    g_pickups[11].y = 5 * 256 + 128;
    g_pickups[11].type = PICKUP_HELMET;
    g_pickups[11].active = true;
    g_pickups[12].x = 4 * 256 + 128;
    g_pickups[12].y = 21 * 256 + 128;
    g_pickups[12].type = PICKUP_AMMO_CLIP;
    g_pickups[12].active = true;
    g_pickups[13].x = 23 * 256 + 128;
    g_pickups[13].y = 14 * 256 + 128;
    g_pickups[13].type = PICKUP_AMMO_CLIP;
    g_pickups[13].active = true;
    g_pickups[14].x = 16 * 256 + 128;
    g_pickups[14].y = 9 * 256 + 128;
    g_pickups[14].type = PICKUP_AMMO_CLIP;
    g_pickups[14].active = true;
    g_pickups[15].x = 4 * 256 + 128;
    g_pickups[15].y = 5 * 256 + 128;
    g_pickups[15].type = PICKUP_WEAPON_ROCKET;
    g_pickups[15].active = true;
}

bool spawnPickup(u8 type, u16 x, u16 y) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        if (!g_pickups[i].active) {
            g_pickups[i].x = x;
            g_pickups[i].y = y;
            g_pickups[i].type = type;
            g_pickups[i].animFrame = 0;
            g_pickups[i].animTimer = 0;
            g_pickups[i].active = true;
            return true;
        }
    }
    return false;
}

void animatePickups(void) {
    u8 i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g_pickups[i];

        /* Deathmatch respawn timer: count down and reactivate */
        if (!p->active && p->respawnTimer > 0) {
            p->respawnTimer--;
            /* Trigger teleport animation right before respawn */
            if (p->respawnTimer == PICKUP_TELEPORT_START) {
                spawnTeleportFX(p->x, p->y);
                playEnemySFX(SFX_TELEPORT, 80);  /* medium distance default */
            }
            if (p->respawnTimer == 0) {
                p->active = true;
                p->animFrame = 0;
                p->animTimer = 0;
            }
            continue;
        }

        if (!p->active) continue;

        /* Only helmet, armor, and keycards animate */
        if (p->type == PICKUP_HELMET) {
            /* 4 frames ping-pong: 0,1,2,3,2,1,0,1,... */
            p->animTimer++;
            if (p->animTimer >= PICKUP_ANIM_RATE) {
                p->animTimer = 0;
                p->animFrame++;
                if (p->animFrame >= 6) p->animFrame = 0;
                /* ping-pong: sequence is 0,1,2,3,2,1 */
            }
        } else if (p->type == PICKUP_ARMOR) {
            /* 3 frames ping-pong: 0,1,2,1,0,1,... */
            p->animTimer++;
            if (p->animTimer >= PICKUP_ANIM_RATE) {
                p->animTimer = 0;
                p->animFrame++;
                if (p->animFrame >= 4) p->animFrame = 0;
                /* ping-pong: sequence is 0,1,2,1 */
            }
        } else if (p->type >= PICKUP_KEY_RED && p->type <= PICKUP_KEY_BLUE) {
            /* 6 frames straight loop: 0,1,2,3,4,5 */
            p->animTimer++;
            if (p->animTimer >= PICKUP_ANIM_RATE) {
                p->animTimer = 0;
                p->animFrame++;
                if (p->animFrame >= 6) p->animFrame = 0;
            }
        }
    }
}

/* Get actual frame index for ping-pong animation */
u8 getPickupAnimFrame(Pickup *p) {
    if (p->type == PICKUP_HELMET) {
        /* Ping-pong: 0,1,2,3,2,1 (animFrame already wraps at 6) */
        static const u8 seq[] = {0, 1, 2, 3, 2, 1};
        return seq[p->animFrame];
    } else if (p->type == PICKUP_ARMOR) {
        /* Ping-pong: 0,1,2,1 (animFrame already wraps at 4) */
        static const u8 seq[] = {0, 1, 2, 1};
        return seq[p->animFrame & 3];
    } else if (p->type >= PICKUP_KEY_RED && p->type <= PICKUP_KEY_BLUE) {
        /* Straight loop: 0,1,2,3,4,5 */
        return p->animFrame;
    }
    return 0; /* static */
}

bool updatePickups(u16 playerX, u16 playerY, u8 *ammo, u8 *health,
                   u16 *armor, u8 *armorType, u8 *shellAmmo) {
    u8 i;
    bool pickedUp = false;

    for (i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g_pickups[i];
        s16 dx, dy;

        if (!p->active) continue;

        dx = (s16)playerX - (s16)p->x;
        dy = (s16)playerY - (s16)p->y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;

        if (dx < (s16)PICKUP_RADIUS && dy < (s16)PICKUP_RADIUS) {
            switch (p->type) {
            case PICKUP_AMMO_CLIP:
                if (*ammo < 200) {
                    *ammo += AMMO_CLIP_AMOUNT;
                    if (*ammo > 200) *ammo = 200;
                } else {
                    continue;
                }
                break;
            case PICKUP_HEALTH_SMALL:
                if (*health < 100) {
                    *health += HEALTH_SMALL_AMOUNT;
                    if (*health > 100) *health = 100;
                } else {
                    continue;
                }
                break;
            case PICKUP_HEALTH_LARGE:
                if (*health < 100) {
                    *health += HEALTH_LARGE_AMOUNT;
                    if (*health > 100) *health = 100;
                } else {
                    continue;
                }
                break;
            case PICKUP_WEAPON_SHOTGUN:
                g_pickedUpWeapon = 3;
                break;
            case PICKUP_HELMET:
                if (*armor >= 200) continue; /* already maxed */
                if (*armorType == 0) *armorType = 1; /* green armor */
                *armor += 1;
                if (*armor > 200) *armor = 200;
                break;
            case PICKUP_ARMOR:
                if (*armor >= 100 && *armorType >= 1) continue;
                *armorType = 1; /* green armor */
                *armor = 100;
                break;
            case PICKUP_SHELLS:
                if (*shellAmmo >= MAX_SHELLS) continue;
                *shellAmmo += SHELLS_BOX_AMOUNT;
                if (*shellAmmo > MAX_SHELLS) *shellAmmo = MAX_SHELLS;
                break;
            case PICKUP_WEAPON_ROCKET:
                g_pickedUpWeapon = 4;  /* W_ROCKET */
                break;
            case PICKUP_KEY_RED:
                g_hasKeyRed = 1;
                break;
            case PICKUP_KEY_YELLOW:
                g_hasKeyYellow = 1;
                break;
            case PICKUP_KEY_BLUE:
                g_hasKeyBlue = 1;
                break;
            case PICKUP_WEAPON_CHAINGUN:
                g_pickedUpWeapon = 5;  /* W_CHAINGUN */
                break;
            }

            /* In deathmatch mode, pickups respawn after a timer instead of disappearing */
            if (g_isMultiplayer && g_gameMode == GAMEMODE_DEATHMATCH) {
                p->active = false;
                p->respawnTimer = PICKUP_RESPAWN_TIME;
            } else {
                p->active = false;
            }
            pickedUp = true;
            /* Only count weapons and helmets toward items% */
            if (p->type == PICKUP_WEAPON_SHOTGUN || p->type == PICKUP_HELMET ||
                p->type == PICKUP_WEAPON_ROCKET || p->type == PICKUP_WEAPON_CHAINGUN)
                g_itemsCollected++;

            g_flashTimer = 3;
            g_flashType = 0;

            if (p->type != PICKUP_WEAPON_SHOTGUN && p->type != PICKUP_WEAPON_ROCKET &&
                p->type != PICKUP_WEAPON_CHAINGUN) {
                playPlayerSFX(SFX_ITEM_UP);
            }
        }
    }

    return pickedUp;
}
