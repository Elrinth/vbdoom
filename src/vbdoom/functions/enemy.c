#include <libgccvb.h>
#include <mem.h>
#include "enemy.h"
#include "RayCasterFixed.h"
#include "RayCaster.h"
#include "doomgfx.h"

/* Global enemy array */
EnemyState g_enemies[MAX_ENEMIES];

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
        /* angle ~= (ady * 128) / adx  (range 0..128 for first octant) */
        angle = (s16)(((s32)ady * 128) / (s32)adx);
    } else {
        /* angle ~= 256 - (adx * 128) / ady */
        angle = 256 - (s16)(((s32)adx * 128) / (s32)ady);
    }

    /* Map to correct quadrant (0-1023) */
    if (dx >= 0 && dy < 0) {
        /* Q4: angle = 1024 - angle */
        angle = 1024 - angle;
    } else if (dx < 0 && dy < 0) {
        /* Q3: angle = 512 + angle */
        angle = 512 + angle;
    } else if (dx < 0 && dy >= 0) {
        /* Q2: angle = 512 - angle */
        angle = 512 - angle;
    }
    /* Q1 (dx>=0, dy>=0): angle stays as-is */

    /* Wrap to 0-1023 */
    angle &= 1023;
    return angle;
}

void initEnemies(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        g_enemies[i].active = false;
        g_enemies[i].state = ES_WALK;
        g_enemies[i].animFrame = 0;
        g_enemies[i].animTimer = 0;
        g_enemies[i].stateTimer = 0;
        g_enemies[i].lastRenderedFrame = 255; /* force first-frame load */
        g_enemies[i].health = 3;
        g_enemies[i].angle = 0;
    }

    /* Place enemy 0 at (896,896) = tile (3,3) center - known open space */
    g_enemies[0].x = 896;
    g_enemies[0].y = 896;
    g_enemies[0].active = true;

    /* Place enemy 1 at (1200,896) - a different open space */
    g_enemies[1].x = 1200;
    g_enemies[1].y = 896;
    g_enemies[1].active = true;

    /* Place enemy 2 at (896,1200) - another open space */
    g_enemies[2].x = 896;
    g_enemies[2].y = 1200;
    g_enemies[2].active = true;
}

void updateEnemies(u16 playerX, u16 playerY, s16 playerA) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        EnemyState *e = &g_enemies[i];
        if (!e->active) continue;
        if (e->state == ES_DEAD) continue;

        /* Compute direction to player (dx, dy in fixed-point) */
        s16 dx = (s16)playerX - (s16)e->x;
        s16 dy = (s16)playerY - (s16)e->y;

        /* Compute distance squared (shifted down to avoid overflow) */
        s32 dist = ((s32)dx * dx + (s32)dy * dy) >> 8;

        /* Update facing angle toward player */
        e->angle = fixedAtan2(dy, dx);

        /* --- State machine --- */
        switch (e->state) {
        case ES_WALK: {
            /* Only move if not too close to player */
            if (dist > (s32)ENEMY_MIN_DIST) {
                /* Move toward player */
                s16 adx = dx < 0 ? -dx : dx;
                s16 ady = dy < 0 ? -dy : dy;
                s16 moveX = 0, moveY = 0;

                /* Normalize movement: move in the dominant axis direction */
                if (adx > ady) {
                    moveX = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED;
                    moveY = (s16)(((s32)dy * ENEMY_SPEED) / (s32)(adx > 0 ? adx : 1));
                } else {
                    moveY = (dy > 0) ? ENEMY_SPEED : -ENEMY_SPEED;
                    moveX = (s16)(((s32)dx * ENEMY_SPEED) / (s32)(ady > 0 ? ady : 1));
                }

                /* Collision check using s16 to detect negative overflow,
                 * matching the player movement code in fPlayerMoveForward */
                s16 nextX = (s16)e->x + moveX;
                s16 nextY = (s16)e->y + moveY;
                u8 prevTileX = (u8)(e->x >> 8);
                u8 prevTileY = (u8)(e->y >> 8);
                u8 tileX, tileY;

                /* Clamp to map bounds (same as player code) */
                if (nextX < 0) nextX = 0;
                if (nextY < 0) nextY = 0;
                tileX = (u8)(nextX >> 8);
                tileY = (u8)(nextY >> 8);
                if (tileX > MAP_X - 1) nextX = (MAP_X << 8) - 1;
                if (tileY > MAP_Y - 1) nextY = (MAP_Y << 8) - 1;
                tileX = (u8)(nextX >> 8);
                tileY = (u8)(nextY >> 8);

                if (IsWall(tileX, tileY) || IsWall(prevTileX, tileY) || IsWall(tileX, prevTileY)) {
                    /* Try sliding along each axis */
                    if (!IsWall(tileX, prevTileY)) {
                        e->x = (u16)nextX;
                    } else if (!IsWall(prevTileX, tileY)) {
                        e->y = (u16)nextY;
                    }
                    /* else: stuck, don't move */
                } else {
                    e->x = (u16)nextX;
                    e->y = (u16)nextY;
                }
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
            /* Stop and shoot periodically */
            e->stateTimer++;
            if (e->stateTimer >= ENEMY_SHOOT_RATE) {
                e->stateTimer = 0;
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
            /* Brief pain flinch, then go back to walk */
            e->stateTimer++;
            if (e->stateTimer >= 10) {
                e->state = ES_WALK;
                e->stateTimer = 0;
                e->animFrame = 0;
            }
            break;

        case ES_DEAD:
            /* Do nothing */
            break;
        }
    }
}

u8 getEnemyDirection(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];

    /* Angle from player TO enemy (what direction the player sees the enemy from) */
    s16 dx = (s16)e->x - (s16)playerX;
    s16 dy = (s16)e->y - (s16)playerY;
    s16 angleToEnemy = fixedAtan2(dy, dx);

    /* Relative angle = (enemy's facing angle) - (angle from player to enemy)
     * This tells us which "side" of the enemy the player sees.
     * 0 = looking at enemy's front, 512 = looking at enemy's back */
    s16 relAngle = e->angle - angleToEnemy;

    /* Wrap to 0-1023 */
    relAngle &= 1023;

    /* Quantize to 8 directions (each is 128 units = 45 degrees) */
    /* Add half a sector (64) for proper rounding, then divide by 128 */
    u8 dir = (u8)(((relAngle + 64) >> 7) & 7);

    return dir;
}

u8 getEnemySpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];
    u8 dir = getEnemyDirection(enemyIdx, playerX, playerY, playerA);

    switch (e->state) {
    case ES_WALK:
        return WALK_FRAMES[dir][e->animFrame];

    case ES_ATTACK:
        return SHOOT_FRAMES[dir];

    case ES_PAIN:
        /* Use walk frame 0 for pain (facing player) */
        return WALK_FRAMES[dir][0];

    case ES_DEAD:
        if (e->animFrame < DEATH_ANIM_FRAMES) {
            return DEATH_FRAMES[e->animFrame];
        }
        return DEATH_FRAMES[DEATH_ANIM_FRAMES - 1]; /* stay on last death frame */

    default:
        return 0;
    }
}
