#include <libgccvb.h>
#include <mem.h>
#include "enemy.h"
#include "pickup.h"
#include "RayCasterFixed.h"
#include "RayCaster.h"
#include "doomgfx.h"

/* Global enemy array */
EnemyState g_enemies[MAX_ENEMIES];

/* Damage feedback globals (read+cleared by game loop each frame) */
u8 g_lastEnemyDamage = 0;
s8 g_lastEnemyDamageDir = 0;

/* Sound event globals (read+cleared by sndplay.c each frame) */
u8 g_enemySndAggro = 0;
u8 g_enemySndShoot = 0;
u8 g_enemySndShootType = 0;
u8 g_enemySndDeath = 0;

/* ---- Simple PRNG (avoids stdlib dependency) ---- */
static u16 rngState = 12345;
static u8 enemyRandom(void) {
    rngState = rngState * 25173 + 13849;
    return (u8)(rngState >> 8);
}

/* ---- Doom-style direction tables (from p_enemy.c) ---- */

#define DIAG_SPEED  ((ENEMY_SPEED * 3) / 4)

static const s16 dirXSpeed[9] = {
    ENEMY_SPEED, DIAG_SPEED, 0, -DIAG_SPEED, -ENEMY_SPEED, -DIAG_SPEED, 0, DIAG_SPEED, 0
};
static const s16 dirYSpeed[9] = {
    0, DIAG_SPEED, ENEMY_SPEED, DIAG_SPEED, 0, -DIAG_SPEED, -ENEMY_SPEED, -DIAG_SPEED, 0
};

static const u8 opposite[9] = {
    DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
    DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

static const u8 diags[4] = {
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};

/* Fixed-point atan2 (0-1023 range) */
static s16 fixedAtan2(s16 dy, s16 dx) {
    s16 angle;
    s16 adx = dx < 0 ? -dx : dx;
    s16 ady = dy < 0 ? -dy : dy;

    if (adx == 0 && ady == 0) return 0;

    if (adx >= ady) {
        angle = (s16)(((s32)ady * 128) / (s32)adx);
    } else {
        angle = 256 - (s16)(((s32)adx * 128) / (s32)ady);
    }

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

/* ---- Distance helper (returns sqrt-ish approx in fixed-point units) ---- */
static u16 approxDist(s16 dx, s16 dy) {
    u16 adx = (u16)(dx < 0 ? -dx : dx);
    u16 ady = (u16)(dy < 0 ? -dy : dy);
    if (adx > ady) return adx + (ady >> 1);
    return ady + (adx >> 1);
}

/* ---- Bounding-box wall collision ---- */
static bool enemyHitsWall(s16 x, s16 y) {
    u8 tx0, tx1, ty0, ty1;
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

/* ---- Doom P_Move ---- */
static bool enemyTryMove(EnemyState *e, u8 idx, u16 playerX, u16 playerY) {
    s16 nextX, nextY, pdx, pdy;
    if (e->movedir > 7) return false;

    nextX = (s16)e->x + dirXSpeed[e->movedir];
    nextY = (s16)e->y + dirYSpeed[e->movedir];

    if (enemyHitsWall(nextX, nextY)) return false;
    if (collidesWithAnyEnemy((u16)nextX, (u16)nextY, ENEMY_RADIUS, idx)) return false;

    pdx = nextX - (s16)playerX;
    pdy = nextY - (s16)playerY;
    if (pdx < 0) pdx = -pdx;
    if (pdy < 0) pdy = -pdy;
    if (pdx < (s16)(ENEMY_RADIUS + PLAYER_RADIUS) &&
        pdy < (s16)(ENEMY_RADIUS + PLAYER_RADIUS)) return false;

    e->x = (u16)nextX;
    e->y = (u16)nextY;
    return true;
}

/* ---- Doom P_NewChaseDir ---- */
static void enemyNewChaseDir(EnemyState *e, u8 idx, u16 playerX, u16 playerY) {
    s16 dx = (s16)playerX - (s16)e->x;
    s16 dy = (s16)playerY - (s16)e->y;
    u8 dX, dY, olddir, turnaround, tdir, temp;

    olddir = e->movedir;
    turnaround = (olddir < 9) ? opposite[olddir] : DI_NODIR;

    if (dx > 10)       dX = DI_EAST;
    else if (dx < -10) dX = DI_WEST;
    else               dX = DI_NODIR;

    if (dy > 10)       dY = DI_NORTH;
    else if (dy < -10) dY = DI_SOUTH;
    else               dY = DI_NODIR;

    if (dX != DI_NODIR && dY != DI_NODIR) {
        e->movedir = diags[((dy < 0) << 1) + (dx > 0)];
        if (e->movedir != turnaround && enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    if (enemyRandom() > 200 || (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx)) {
        temp = dX; dX = dY; dY = temp;
    }

    if (dX == turnaround) dX = DI_NODIR;
    if (dY == turnaround) dY = DI_NODIR;

    if (dX != DI_NODIR) {
        e->movedir = dX;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    if (dY != DI_NODIR) {
        e->movedir = dY;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    if (olddir != DI_NODIR) {
        e->movedir = olddir;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++) {
        if (tdir != turnaround) {
            e->movedir = tdir;
            if (enemyTryMove(e, idx, playerX, playerY)) {
                e->movecount = enemyRandom() & 15;
                return;
            }
        }
    }

    if (turnaround != DI_NODIR) {
        e->movedir = turnaround;
        if (enemyTryMove(e, idx, playerX, playerY)) {
            e->movecount = enemyRandom() & 15;
            return;
        }
    }

    e->movedir = DI_NODIR;
}

/* ---- Line-of-sight check ---- */
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
 * Doom-style P_CheckMissileRange: determines if an enemy decides to fire.
 * Returns true if the enemy should attack this frame.
 * Farther enemies are less likely to fire per frame.
 */
static bool checkMissileRange(EnemyState *e, u16 playerX, u16 playerY) {
    s16 dx = (s16)playerX - (s16)e->x;
    s16 dy = (s16)playerY - (s16)e->y;
    u16 dist;

    /* Must have line of sight */
    if (!hasLineOfSight(e->x, e->y, playerX, playerY)) return false;

    /* Approximate distance in fixed-point units */
    dist = approxDist(dx, dy);

    /* Convert to Doom-like units: subtract 64 (no melee state) and 128 */
    if (dist > 192) dist -= 192;
    else dist = 1;

    /* Scale down to 0-255 range (dist/4, capped at 200) */
    dist >>= 2;
    if (dist > 200) dist = 200;

    /* Random check: if random < dist, don't fire (farther = less likely) */
    if (enemyRandom() < (u8)dist) return false;

    return true;
}

void initEnemies(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        g_enemies[i].active = false;
        g_enemies[i].state = ES_IDLE;
        g_enemies[i].animFrame = 0;
        g_enemies[i].animTimer = 0;
        g_enemies[i].stateTimer = 0;
        g_enemies[i].lastRenderedFrame = 255;
        g_enemies[i].health = ZOMBIE_HEALTH;
        g_enemies[i].angle = 0;
        g_enemies[i].movedir = DI_NODIR;
        g_enemies[i].movecount = 0;
        g_enemies[i].enemyType = ETYPE_ZOMBIEMAN;
    }

    /* Enemy 0: zombieman, main hall */
    g_enemies[0].x = 15 * 256 + 128;
    g_enemies[0].y = 21 * 256 + 128;
    g_enemies[0].active = true;
    g_enemies[0].animFrame = 0;
    g_enemies[0].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[0].health = ZOMBIE_HEALTH;

    /* Enemy 1: zombieman, zigzag room */
    g_enemies[1].x = 8 * 256 + 128;
    g_enemies[1].y = 14 * 256 + 128;
    g_enemies[1].active = true;
    g_enemies[1].animFrame = 1;
    g_enemies[1].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[1].health = ZOMBIE_HEALTH;

    /* Enemy 2: sergeant (shotgun guy), zigzag room */
    g_enemies[2].x = 24 * 256 + 128;
    g_enemies[2].y = 13 * 256 + 128;
    g_enemies[2].active = true;
    g_enemies[2].animFrame = 2;
    g_enemies[2].enemyType = ETYPE_SERGEANT;
    g_enemies[2].health = SGT_HEALTH;
}

void alertAllEnemies(void) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].active && g_enemies[i].state == ES_IDLE) {
            g_enemies[i].state = ES_WALK;
            g_enemies[i].animFrame = 0;
            g_enemies[i].animTimer = 0;
            g_enemies[i].movedir = DI_NODIR;
            g_enemies[i].movecount = 0;
            /* Aggro sound is triggered per-enemy in updateEnemies() when
             * they transition from ES_IDLE to ES_WALK (has player coords) */
        }
    }
}

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
 * Helper: apply damage to an enemy, handle kill/pain/drops/sounds.
 * Returns true if the enemy was killed.
 */
static bool applyDamageToEnemy(EnemyState *e, u8 damage, u16 playerX, u16 playerY) {
    if (e->health <= damage) {
        /* Kill enemy */
        e->health = 0;
        e->state = ES_DEAD;
        e->animFrame = 0;
        e->animTimer = 0;

        /* Drop item based on enemy type */
        if (e->enemyType == ETYPE_SERGEANT) {
            spawnPickup(PICKUP_WEAPON_SHOTGUN, e->x, e->y);
        } else {
            spawnPickup(PICKUP_AMMO_CLIP, e->x, e->y);
        }

        /* Death sound event */
        {
            s16 ddx = (s16)e->x - (s16)playerX;
            s16 ddy = (s16)e->y - (s16)playerY;
            u16 d = approxDist(ddx, ddy);
            g_enemySndDeath = (u8)(d > 4080 ? 255 : d >> 4);
            if (g_enemySndDeath == 0) g_enemySndDeath = 1;
        }
        return true;
    } else {
        u8 painchance;
        e->health -= damage;
        /* Pain chance check (Doom-style) */
        painchance = (e->enemyType == ETYPE_SERGEANT) ? SGT_PAINCHANCE : ZOMBIE_PAINCHANCE;
        if (enemyRandom() < painchance) {
            e->state = ES_PAIN;
            e->stateTimer = 0;
        }
        return false;
    }
}

/*
 * Single-pellet hitscan: find closest enemy within a cone around the given angle.
 * Returns enemy index or 255 if no hit.
 */
static u8 hitscanFindEnemy(u16 playerX, u16 playerY, s16 aimAngle, s16 halfCone) {
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

        angleToEnemy = (256 - fixedAtan2(dy, dx)) & 1023;
        angleDiff = (angleToEnemy - aimAngle) & 1023;
        if (angleDiff > 512) angleDiff -= 1024;
        if (angleDiff < 0) angleDiff = -angleDiff;

        if (angleDiff > halfCone) continue;

        dist = ((s32)dx * dx + (s32)dy * dy) >> 8;
        if (dist > 10000) continue;

        if (!hasLineOfSight(playerX, playerY, e->x, e->y)) continue;

        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

/*
 * Hitscan: player fires. weaponType: 1=fist, 2=pistol, 3=shotgun.
 *
 * Doom damage formulae:
 *   Fist:    (P_Random()%10 + 1) * 2 = 2-20
 *   Pistol:  (P_Random()%5  + 1) * 3 = 3-15
 *   Shotgun: 7 pellets with spread, each (P_Random()%5 + 1) * 3 = 3-15
 *            Total shotgun potential: 21-105 damage
 *
 * Shotgun spread: (P_Random()-P_Random())<<18 BAM ≈ +-5.6 degrees ≈ +-16 in our 1024-unit system.
 */
u8 playerShoot(u16 playerX, u16 playerY, s16 playerA, u8 weaponType) {
    u8 hitIdx = 255;

    alertAllEnemies();

    if (weaponType == 3) {
        /* ---- Shotgun: 7 pellets with angular spread ---- */
        u8 pelletDmg[MAX_ENEMIES];
        u8 p, i;
        u8 anyHit = 255;
        u8 anyKill = 255;

        for (i = 0; i < MAX_ENEMIES; i++) pelletDmg[i] = 0;

        for (p = 0; p < 7; p++) {
            s16 spread, pelletAngle;
            u8 target;

            /* Doom-style spread: (P_Random()-P_Random()) >> 4 in our angle units */
            spread = ((s16)enemyRandom() - (s16)enemyRandom()) >> 4;
            pelletAngle = (playerA + spread) & 1023;

            /* Each pellet does narrow hitscan (half-cone = 10 ≈ 3.5 degrees) */
            target = hitscanFindEnemy(playerX, playerY, pelletAngle, 10);

            if (target != 255) {
                /* Doom shotgun pellet damage: (P_Random()%5 + 1) * 3 = 3,6,9,12,15 */
                pelletDmg[target] += ((enemyRandom() % 5) + 1) * 3;
                if (anyHit == 255) anyHit = target;
            }
        }

        /* Apply accumulated damage to each hit enemy */
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (pelletDmg[i] == 0) continue;
            if (applyDamageToEnemy(&g_enemies[i], pelletDmg[i], playerX, playerY)) {
                anyKill = i; /* at least one enemy killed */
            }
        }

        hitIdx = (anyKill != 255) ? anyKill : anyHit;
    } else {
        /* ---- Pistol / Fist: single hitscan ---- */
        u8 target = hitscanFindEnemy(playerX, playerY, playerA & 1023, 32);

        if (target != 255) {
            u8 damage;
            if (weaponType == 1) {
                /* Fist: (P_Random()%10 + 1) * 2 = 2-20 */
                damage = ((enemyRandom() % 10) + 1) * 2;
            } else {
                /* Pistol: (P_Random()%5 + 1) * 3 = 3-15 */
                damage = ((enemyRandom() % 5) + 1) * 3;
            }
            applyDamageToEnemy(&g_enemies[target], damage, playerX, playerY);
            hitIdx = target;
        }
    }

    return hitIdx;
}

/*
 * Enemy fires at player. Applies DOOM-style angular spread for miss chance.
 * Zombieman: 1 bullet, 3-15 damage.
 * Sergeant: 3 pellets, each 3-15 damage.
 */
static void enemyFireAtPlayer(EnemyState *e, u16 playerX, u16 playerY, s16 playerA) {
    s16 dx = (s16)playerX - (s16)e->x;
    s16 dy = (s16)playerY - (s16)e->y;
    u16 dist = approxDist(dx, dy);
    u8 pellets, p;
    u8 totalDamage = 0;

    /* Must have line of sight */
    if (!hasLineOfSight(e->x, e->y, playerX, playerY)) return;

    pellets = (e->enemyType == ETYPE_SERGEANT) ? 3 : 1;

    for (p = 0; p < pellets; p++) {
        /* Doom-style spread: (P_Random() - P_Random()) scaled to angle range.
         * Spread range is roughly +-45 degrees in Doom (+-128 in our 0-1023 system).
         * At close range this almost always hits; at long range the subtended
         * angle to the player is small so spread causes misses. */
        s16 spread = (s16)enemyRandom() - (s16)enemyRandom();
        /* spread is -255..+255. Scale to +-64 (roughly +-22 degrees) */
        spread >>= 2;

        /* The "hit cone" is the angular size of the player from this distance.
         * Player is ~128 units wide, so half-angle = atan(64/dist) ≈ 64*128/dist.
         * Simplify: if |spread| < 32 + 8192/dist, it's a hit. */
        {
            s16 hitCone = 32;
            if (dist > 0) hitCone = (s16)(32 + (8192 / (s32)dist));
            if (hitCone > 90) hitCone = 90;

            if (spread < 0) spread = -spread;
            if (spread < hitCone) {
                /* Hit! Doom damage formula */
                totalDamage += 3 + (enemyRandom() % 13);
            }
        }
    }

    if (totalDamage > 0) {
        g_lastEnemyDamage = totalDamage;

        /* Compute damage direction relative to player facing */
        {
            s16 angleToEnemy = (256 - fixedAtan2(dy, dx)) & 1023;
            s16 relAngle = angleToEnemy - playerA;
            if (relAngle > 512)  relAngle -= 1024;
            if (relAngle < -512) relAngle += 1024;
            if (relAngle > 128)       g_lastEnemyDamageDir =  1;
            else if (relAngle < -128) g_lastEnemyDamageDir = -1;
            else                      g_lastEnemyDamageDir =  0;
        }
    }

    /* Shoot sound event */
    {
        u8 sdist = (u8)(dist > 4080 ? 255 : dist >> 4);
        if (sdist == 0) sdist = 1;
        g_enemySndShoot = sdist;
        g_enemySndShootType = e->enemyType;
    }
}

void updateEnemies(u16 playerX, u16 playerY, s16 playerA) {
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        EnemyState *e = &g_enemies[i];
        s16 dx, dy;
        s32 dist;
        if (!e->active) continue;

        dx = (s16)playerX - (s16)e->x;
        dy = (s16)playerY - (s16)e->y;
        dist = ((s32)dx * dx + (s32)dy * dy) >> 8;

        switch (e->state) {
        case ES_IDLE: {
            if (dist < (s32)ENEMY_SIGHT_DIST) {
                e->state = ES_WALK;
                e->animFrame = 0;
                e->animTimer = 0;
                e->movedir = DI_NODIR;
                e->movecount = 0;
                e->angle = fixedAtan2(dy, dx);
                /* Aggro sound */
                {
                    u16 d = approxDist(dx, dy);
                    g_enemySndAggro = (u8)(d > 4080 ? 255 : d >> 4);
                    if (g_enemySndAggro == 0) g_enemySndAggro = 1;
                }
            }
            break;
        }

        case ES_WALK: {
            /* Chase movement */
            if (e->movecount > 0) {
                e->movecount--;
                if (!enemyTryMove(e, i, playerX, playerY)) {
                    enemyNewChaseDir(e, i, playerX, playerY);
                }
            } else {
                enemyNewChaseDir(e, i, playerX, playerY);
            }

            if (e->movedir < 8) {
                e->angle = (s16)(e->movedir * 128);
            }

            /* Check if we should attack (Doom-style ranged check) */
            if (dist < (s32)ENEMY_ATTACK_DIST &&
                checkMissileRange(e, playerX, playerY)) {
                e->state = ES_ATTACK;
                e->stateTimer = 0;
                e->animFrame = 0;
            }

            /* Walk animation */
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
            /* Face the player while attacking */
            e->angle = fixedAtan2(dy, dx);

            e->stateTimer++;
            if (e->stateTimer >= ENEMY_SHOOT_RATE) {
                e->stateTimer = 0;
                e->animFrame = e->animFrame ? 0 : 1;

                /* Fire at player with spread-based accuracy */
                enemyFireAtPlayer(e, playerX, playerY, playerA);

                /* After firing, return to walk (Doom pattern: shoot then chase) */
                e->state = ES_WALK;
                e->animTimer = 0;
                e->animFrame = 0;
                e->movecount = 8 + (enemyRandom() & 15); /* chase a bit before next shot */
            }
            break;
        }

        case ES_PAIN:
            e->angle = fixedAtan2(dy, dx);
            e->stateTimer++;
            if (e->stateTimer >= 10) {
                e->state = ES_WALK;
                e->stateTimer = 0;
                e->animFrame = 0;
            }
            break;

        case ES_DEAD:
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
    s16 dx = (s16)e->x - (s16)playerX;
    s16 dy = (s16)e->y - (s16)playerY;
    s16 angleToEnemy = fixedAtan2(dy, dx);
    s16 relAngle = (angleToEnemy - e->angle + 576) & 1023;
    u8 dir = (u8)((relAngle >> 7) & 7);
    return dir;
}

u8 getEnemySpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];
    u8 dir = getEnemyDirection(enemyIdx, playerX, playerY, playerA);

    switch (e->state) {
    case ES_IDLE:
        return WALK_FRAMES[dir][0];

    case ES_WALK:
        return WALK_FRAMES[dir][e->animFrame];

    case ES_ATTACK: {
        u8 pose = e->animFrame;
        if (pose >= ATTACK_ANIM_FRAMES) pose = 0;
        return ATTACK_FRAMES[dir][pose];
    }

    case ES_PAIN:
        return PAIN_FRAMES[dir];

    case ES_DEAD:
        if (e->animFrame < DEATH_ANIM_FRAMES) {
            return DEATH_FRAMES[e->animFrame];
        }
        return DEATH_FRAMES[DEATH_ANIM_FRAMES - 1];

    default:
        return 0;
    }
}
