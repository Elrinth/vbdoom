#include <libgccvb.h>
#include <mem.h>
#include "enemy.h"
#include "pickup.h"
#include "RayCasterFixed.h"
#include "RayCaster.h"
#include "doomgfx.h"

/* Global enemy array */
EnemyState g_enemies[MAX_ENEMIES];

/* ---- Simple PRNG (avoids stdlib dependency) ---- */
static u16 rngState = 12345;
static u8 enemyRandom(void) {
    rngState = rngState * 25173 + 13849;
    return (u8)(rngState >> 8);
}

/* ---- Doom-style direction tables (from p_enemy.c) ---- */

/* Diagonal speed = ENEMY_SPEED * 0.7071 ≈ 6 (when ENEMY_SPEED=8) */
#define DIAG_SPEED  ((ENEMY_SPEED * 3) / 4)

/* Movement delta per direction: E, NE, N, NW, W, SW, S, SE, NODIR */
static const s16 dirXSpeed[9] = {
    ENEMY_SPEED, DIAG_SPEED, 0, -DIAG_SPEED, -ENEMY_SPEED, -DIAG_SPEED, 0, DIAG_SPEED, 0
};
static const s16 dirYSpeed[9] = {
    0, DIAG_SPEED, ENEMY_SPEED, DIAG_SPEED, 0, -DIAG_SPEED, -ENEMY_SPEED, -DIAG_SPEED, 0
};

/* Opposite direction lookup (Doom P_NewChaseDir) */
static const u8 opposite[9] = {
    DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
    DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

/* Diagonal direction lookup: [((dy<0)<<1) + (dx>0)] */
static const u8 diags[4] = {
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};

/*
 * Simple fixed-point atan2 approximation.
 * Returns angle in 0-1023 range (full circle).
 * Uses octant symmetry + linear approximation.
 */
static s16 fixedAtan2(s16 dy, s16 dx) {
    s16 angle;
    s16 adx = dx < 0 ? -dx : dx;
    s16 ady = dy < 0 ? -dy : dy;

    /* Avoid division by zero */
    if (adx == 0 && ady == 0) return 0;

    /* Approximate atan(y/x) in the first octant, scaled to 0-128 (= 45 degrees) */
    if (adx >= ady) {
        angle = (s16)(((s32)ady * 128) / (s32)adx);
    } else {
        angle = 256 - (s16)(((s32)adx * 128) / (s32)ady);
    }

    /* Map to correct quadrant (0-1023) */
    if (dx >= 0 && dy < 0) {
        angle = 1024 - angle;
    } else if (dx < 0 && dy < 0) {
        angle = 512 + angle;
    } else if (dx < 0 && dy >= 0) {
        angle = 512 - angle;
    }

    angle &= 1023;
    return angle;
}

/* ---- Bounding-box wall collision (checks all 4 corners) ---- */
static bool enemyHitsWall(s16 x, s16 y) {
    u8 tx0, tx1, ty0, ty1;
    /* Out of bounds = wall */
    if (x < ENEMY_RADIUS || y < ENEMY_RADIUS) return true;
    if ((x + ENEMY_RADIUS) >> 8 >= MAP_X) return true;
    if ((y + ENEMY_RADIUS) >> 8 >= MAP_Y) return true;
    tx0 = (u8)((x - ENEMY_RADIUS) >> 8);
    tx1 = (u8)((x + ENEMY_RADIUS) >> 8);
    ty0 = (u8)((y - ENEMY_RADIUS) >> 8);
    ty1 = (u8)((y + ENEMY_RADIUS) >> 8);
    return IsWall(tx0, ty0) || IsWall(tx1, ty0) ||
           IsWall(tx0, ty1) || IsWall(tx1, ty1);
}

/* ---- Doom P_Move: try to move enemy in its current movedir ---- */
static bool enemyTryMove(EnemyState *e, u8 idx, u16 playerX, u16 playerY) {
    s16 nextX, nextY, pdx, pdy;
    if (e->movedir > 7) return false; /* DI_NODIR */

    nextX = (s16)e->x + dirXSpeed[e->movedir];
    nextY = (s16)e->y + dirYSpeed[e->movedir];

    /* Wall collision (bounding box) */
    if (enemyHitsWall(nextX, nextY)) return false;

    /* Enemy-enemy collision */
    if (collidesWithAnyEnemy((u16)nextX, (u16)nextY, ENEMY_RADIUS, idx)) return false;

    /* Player collision */
    pdx = nextX - (s16)playerX;
    pdy = nextY - (s16)playerY;
    if (pdx < 0) pdx = -pdx;
    if (pdy < 0) pdy = -pdy;
    if (pdx < (s16)(ENEMY_RADIUS + PLAYER_RADIUS) &&
        pdy < (s16)(ENEMY_RADIUS + PLAYER_RADIUS)) return false;

    /* Move is clear */
    e->x = (u16)nextX;
    e->y = (u16)nextY;
    return true;
}

/* ---- Doom P_NewChaseDir: pick best direction toward player ---- */
static void enemyNewChaseDir(EnemyState *e, u8 idx, u16 playerX, u16 playerY) {
    s16 dx = (s16)playerX - (s16)e->x;
    s16 dy = (s16)playerY - (s16)e->y;
    u8 dX, dY, olddir, turnaround, tdir, temp;

    olddir = e->movedir;
    turnaround = (olddir < 9) ? opposite[olddir] : DI_NODIR;

    /* Preferred X direction */
    if (dx > 10)       dX = DI_EAST;
    else if (dx < -10) dX = DI_WEST;
    else               dX = DI_NODIR;

    /* Preferred Y direction */
    if (dy > 10)       dY = DI_NORTH;
    else if (dy < -10) dY = DI_SOUTH;
    else               dY = DI_NODIR;

    /* Try diagonal first (direct path) */
    if (dX != DI_NODIR && dY != DI_NODIR) {
        e->movedir = diags[((dy < 0) << 1) + (dx > 0)];
        if (e->movedir != turnaround && enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    /* Randomly swap X/Y preference (adds variety, matches Doom) */
    if (enemyRandom() > 200 || (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx)) {
        temp = dX; dX = dY; dY = temp;
    }

    /* Never reverse into turnaround */
    if (dX == turnaround) dX = DI_NODIR;
    if (dY == turnaround) dY = DI_NODIR;

    /* Try preferred X */
    if (dX != DI_NODIR) {
        e->movedir = dX;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    /* Try preferred Y */
    if (dY != DI_NODIR) {
        e->movedir = dY;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    /* Try continuing in old direction */
    if (olddir != DI_NODIR) {
        e->movedir = olddir;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    /* Exhaustive search: try all 8 directions */
    for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++) {
        if (tdir != turnaround) {
            e->movedir = tdir;
            if (enemyTryMove(e, idx, playerX, playerY)) {
                e->movecount = enemyRandom() & 15;
                return;
            }
        }
    }

    /* Last resort: turnaround */
    if (turnaround != DI_NODIR) {
        e->movedir = turnaround;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    e->movedir = DI_NODIR; /* completely stuck */
}

void initEnemies(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        g_enemies[i].active = false;
        g_enemies[i].state = ES_IDLE;  /* start idle, like Doom */
        g_enemies[i].animFrame = 0;
        g_enemies[i].animTimer = 0;
        g_enemies[i].stateTimer = 0;
        g_enemies[i].lastRenderedFrame = 255; /* force first-frame load */
        g_enemies[i].health = 20; /* Doom zombieman HP */
        g_enemies[i].angle = 0;
        g_enemies[i].movedir = DI_NODIR;
        g_enemies[i].movecount = 0;
    }

    /* Place enemy 0 at (896,896) = tile (3,3) center - known open space */
    g_enemies[0].x = 896;
    g_enemies[0].y = 896;
    g_enemies[0].active = true;
    g_enemies[0].animFrame = 0;

    /* Place enemy 1 farther away at tile (6,3) */
    g_enemies[1].x = 1664;
    g_enemies[1].y = 896;
    g_enemies[1].active = true;
    g_enemies[1].animFrame = 1;  /* stagger animation so they don't walk in lockstep */

    /* Place enemy 2 at tile (3,6) */
    g_enemies[2].x = 896;
    g_enemies[2].y = 1664;
    g_enemies[2].active = true;
    g_enemies[2].animFrame = 2;  /* stagger animation */
}

/* Alert all active idle enemies (e.g. player fired a weapon) */
void alertAllEnemies(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active && g_enemies[i].state == ES_IDLE) {
            g_enemies[i].state = ES_WALK;
            g_enemies[i].animFrame = 0;
            g_enemies[i].animTimer = 0;
            g_enemies[i].movedir = DI_NODIR;
            g_enemies[i].movecount = 0;
        }
    }
}

/* Doom-style AABB collision check.
 * Returns true if position (x,y) with radius myRadius overlaps any active enemy.
 * Pass skipIdx=255 to check all enemies (used for player collision).
 * Pass the enemy's own index to skip self (used for enemy-enemy collision).
 */
bool collidesWithAnyEnemy(u16 x, u16 y, u16 myRadius, u8 skipIdx) {
    u8 j;
    s16 minDist = (s16)(myRadius + ENEMY_RADIUS);
    for (j = 0; j < MAX_ENEMIES; j++) {
        s16 adx, ady;
        if (j == skipIdx) continue;
        if (!g_enemies[j].active || g_enemies[j].state == ES_DEAD) continue;
        adx = (s16)x - (s16)g_enemies[j].x;
        ady = (s16)y - (s16)g_enemies[j].y;
        if (adx < 0) adx = -adx;
        if (ady < 0) ady = -ady;
        if (adx < minDist && ady < minDist) return true;
    }
    return false;
}

/*
 * Simple line-of-sight check: march from player to enemy,
 * checking if any tile along the way is a wall.
 * Returns true if the path is clear (no walls blocking).
 */
static bool hasLineOfSight(u16 px, u16 py, u16 ex, u16 ey) {
    s16 dx = (s16)ex - (s16)px;
    s16 dy = (s16)ey - (s16)py;
    u8 i;
    for (i = 1; i < 8; i++) {
        s16 cx = (s16)px + (dx * i) / 8;
        s16 cy = (s16)py + (dy * i) / 8;
        u8 tx = (u8)(cx >> 8);
        u8 ty = (u8)(cy >> 8);
        if (IsWall(tx, ty)) return false;
    }
    return true;
}

/*
 * Hitscan: player fires a shot in their facing direction.
 * Finds the closest enemy within an aiming cone (~11 degrees each side)
 * that has line-of-sight, and applies damage.
 * Returns index of hit enemy, or 255 if miss.
 *
 * Based on Doom's P_AimLineAttack + P_LineAttack (simplified):
 * - Doom traces a ray with P_PathTraverse checking for shootable things
 * - We approximate with an angular cone + distance + LOS check
 */
u8 playerShoot(u16 playerX, u16 playerY, s16 playerA) {
    u8 i;
    s32 bestDist = 0x7FFFFFFF;
    u8 bestIdx = 255;

    for (i = 0; i < MAX_ENEMIES; i++) {
        EnemyState *e = &g_enemies[i];
        s16 dx, dy, angleToEnemy, angleDiff;
        s32 dist;

        if (!e->active || e->state == ES_DEAD) continue;

        dx = (s16)e->x - (s16)playerX;
        dy = (s16)e->y - (s16)playerY;

        /* Angle from player to enemy.
         * fixedAtan2 returns standard-math angles (0=east, 256=north, CCW).
         * playerA uses raycaster convention (0=north, 256=east, CW).
         * Convert: raycasterAngle = (256 - fixedAtan2Angle) & 1023
         */
        angleToEnemy = (256 - fixedAtan2(dy, dx)) & 1023;

        /* Angle difference: how far off-center the enemy is from crosshair */
        angleDiff = (angleToEnemy - playerA) & 1023;
        if (angleDiff > 512) angleDiff -= 1024;
        /* angleDiff is now -512..+511 */
        if (angleDiff < 0) angleDiff = -angleDiff;

        /* Aiming cone: ~11 degrees each side (32 in 0-1023 range) */
        if (angleDiff > 32) continue;

        /* Distance check (squared / 256) */
        dist = ((s32)dx * dx + (s32)dy * dy) >> 8;
        if (dist > 10000) continue; /* beyond effective range */

        /* Line-of-sight check (don't shoot through walls) */
        if (!hasLineOfSight(playerX, playerY, e->x, e->y)) continue;

        /* Pick closest qualifying enemy */
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    /* Gunshot noise always wakes all enemies (hit or miss) */
    alertAllEnemies();

    if (bestIdx != 255) {
        EnemyState *e = &g_enemies[bestIdx];

        /* Doom pistol damage: (P_Random()%5 + 1) * 3 = 3-15 */
        u8 damage = 3 + (enemyRandom() % 13); /* 3-15 damage */

        if (e->health <= damage) {
            /* Kill enemy */
            e->health = 0;
            e->state = ES_DEAD;
            e->animFrame = 0;
            e->animTimer = 0;
            /* Drop ammo clip at enemy's death position (like Doom zombieman) */
            spawnPickup(PICKUP_AMMO_CLIP, e->x, e->y);
        } else {
            e->health -= damage;
            /* Enter pain state (interrupts current action, like Doom) */
            e->state = ES_PAIN;
            e->stateTimer = 0;
        }
    }

    return bestIdx;
}

void updateEnemies(u16 playerX, u16 playerY, s16 playerA) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        EnemyState *e = &g_enemies[i];
        s16 dx, dy;
        s32 dist;
        if (!e->active) continue;

        /* Compute direction to player (dx, dy in fixed-point) */
        dx = (s16)playerX - (s16)e->x;
        dy = (s16)playerY - (s16)e->y;

        /* Compute distance squared (shifted down to avoid overflow) */
        dist = ((s32)dx * dx + (s32)dy * dy) >> 8;

        /* --- State machine --- */
        switch (e->state) {
        case ES_IDLE: {
            /*
             * Doom A_Look: enemy stands still, facing a spawn direction.
             * Does NOT face the player (stays facing current angle).
             * Becomes alert when:
             *   - Player gets within sight distance (simplified LOS check)
             *   - alertAllEnemies() is called (e.g. gunshot)
             */
            if (dist < (s32)ENEMY_SIGHT_DIST) {
                /* Player is close enough - wake up! */
                e->state = ES_WALK;
                e->animFrame = 0;
                e->animTimer = 0;
                e->movedir = DI_NODIR;
                e->movecount = 0;
                /* Snap to face player on alert */
                e->angle = fixedAtan2(dy, dx);
            }
            break;
        }

        case ES_WALK: {
            /*
             * Doom A_Chase style movement:
             * 1. Decrement movecount. While > 0, keep moving in current dir.
             * 2. When movecount reaches 0, or current direction is blocked,
             *    pick a new direction via P_NewChaseDir.
             * 3. Movement uses bounding-box wall collision + entity collision.
             */
            if (e->movecount > 0) {
                e->movecount--;
                if (!enemyTryMove(e, i, playerX, playerY)) {
                    enemyNewChaseDir(e, i, playerX, playerY);
                }
            } else {
                enemyNewChaseDir(e, i, playerX, playerY);
            }

            /*
             * Doom A_Chase: turn toward movement direction (not instant snap).
             * In Doom, this is gradual (ANG90/2 per tic). For simplicity,
             * we snap to the movedir angle. This gives proper directional
             * sprites instead of always showing the front.
             */
            if (e->movedir < 8) {
                e->angle = (s16)(e->movedir * 128);
            }

            /* Switch to attack if close enough */
            if (dist < (s32)ENEMY_ATTACK_DIST) {
                e->state = ES_ATTACK;
                e->stateTimer = 0;
                e->animFrame = 0;
            }

            /* Walk animation timer */
            e->animTimer++;
            if (e->animTimer >= ENEMY_WALK_RATE) {
                e->animTimer = 0;
                e->animFrame++;
                if (e->animFrame >= WALK_ANIM_FRAMES) {
                    e->animFrame = 0;
                }
            }
            break;
        }

        case ES_ATTACK: {
            /* Face the player while attacking (aiming) */
            e->angle = fixedAtan2(dy, dx);

            /* Stop and shoot periodically, animate between 2 attack poses */
            e->stateTimer++;
            if (e->stateTimer >= ENEMY_SHOOT_RATE) {
                e->stateTimer = 0;
                /* Toggle between attack pose 0 and 1 (E and F frames) */
                e->animFrame = e->animFrame ? 0 : 1;
                /* TODO: deal damage to player here */
            }

            /* If player moves away, go back to walking */
            if (dist >= (s32)(ENEMY_ATTACK_DIST + 100)) {
                e->state = ES_WALK;
                e->animTimer = 0;
                e->animFrame = 0;
            }
            break;
        }

        case ES_PAIN:
            /* Face the player while flinching */
            e->angle = fixedAtan2(dy, dx);
            /* Brief pain flinch, then go back to walk */
            e->stateTimer++;
            if (e->stateTimer >= 10) {
                e->state = ES_WALK;
                e->stateTimer = 0;
                e->animFrame = 0;
            }
            break;

        case ES_DEAD:
            /* Animate through death frames, then stay on last frame */
            e->animTimer++;
            if (e->animTimer >= 6) {
                e->animTimer = 0;
                if (e->animFrame < DEATH_ANIM_FRAMES - 1) {
                    e->animFrame++;
                }
            }
            break;
        }
    }
}

u8 getEnemyDirection(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];

    /* Angle from player TO enemy (like Doom's R_PointToAngle(thing->x, thing->y)) */
    s16 dx = (s16)e->x - (s16)playerX;
    s16 dy = (s16)e->y - (s16)playerY;
    s16 angleToEnemy = fixedAtan2(dy, dx);

    /*
     * Doom's R_ProjectSprite rotation formula:
     *   rot = (ang - thing->angle + (ANG45/2)*9) >> 29
     *
     * In our 10-bit angle system (0-1023):
     *   ANG45 = 128, so (ANG45/2)*9 = 64*9 = 576
     *   576 = 512 (180deg flip) + 64 (half-sector rounding)
     *
     * This correctly maps:
     *   rot 0 = front (facing viewer)
     *   rot 1 = front-right (clockwise)
     *   rot 2 = right, etc.
     */
    s16 relAngle = (angleToEnemy - e->angle + 576) & 1023;
    u8 dir = (u8)((relAngle >> 7) & 7);

    return dir;
}

u8 getEnemySpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];
    u8 dir = getEnemyDirection(enemyIdx, playerX, playerY, playerA);

    switch (e->state) {
    case ES_IDLE:
        /* Idle: standing still, use first walk pose for current direction */
        return WALK_FRAMES[dir][0];

    case ES_WALK:
        return WALK_FRAMES[dir][e->animFrame];

    case ES_ATTACK: {
        /* Animate between the 2 attack poses (E and F) for this direction */
        u8 pose = e->animFrame;
        if (pose >= ATTACK_ANIM_FRAMES) pose = 0;
        return ATTACK_FRAMES[dir][pose];
    }

    case ES_PAIN:
        /* Pain flinch: use the pain frame for this direction */
        return PAIN_FRAMES[dir];

    case ES_DEAD:
        if (e->animFrame < DEATH_ANIM_FRAMES) {
            return DEATH_FRAMES[e->animFrame];
        }
        return DEATH_FRAMES[DEATH_ANIM_FRAMES - 1]; /* stay on last death frame */

    default:
        return 0;
    }
}
