#include <libgccvb.h>
#include <mem.h>
#include "projectile.h"
#include "RayCaster.h"
#include "RayCasterFixed.h"
#include "enemy.h"
#include "sndplay.h"
#include "../assets/audio/doom_sfx.h"

/* Global projectile array */
Projectile g_projectiles[MAX_PROJECTILES];

/* Damage feedback (shared with enemy.c) */
extern u8 g_lastEnemyDamage;
extern s8 g_lastEnemyDamageDir;

/* Simple PRNG (share with enemy.c) */
static u16 projRng = 54321;
static u8 projRandom(void) {
    projRng = projRng * 25173 + 13849;
    return (u8)(projRng >> 8);
}

/* Fixed-point atan2 (0-1023 range) - same as enemy.c */
static s16 projAtan2(s16 dy, s16 dx) {
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

    return angle & 1023;
}

void initProjectiles(void) {
    u8 i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        g_projectiles[i].state = PROJ_DEAD;
    }
}

void spawnFireball(u16 sx, u16 sy, u16 tx, u16 ty, u8 sourceIdx) {
    u8 i;
    Projectile *p;
    s16 dx, dy;
    s32 dist;
    s16 ndx, ndy;

    /* Find a free slot */
    p = 0;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (g_projectiles[i].state == PROJ_DEAD) {
            p = &g_projectiles[i];
            break;
        }
    }
    if (!p) return; /* No free slot */

    /* Compute direction toward target */
    dx = (s16)tx - (s16)sx;
    dy = (s16)ty - (s16)sy;

    /* Normalize to FIREBALL_SPEED magnitude */
    dist = (s32)dx * dx + (s32)dy * dy;
    if (dist == 0) {
        ndx = FIREBALL_SPEED;
        ndy = 0;
    } else {
        /* Approximate: speed * dx / sqrt(dist) */
        u16 adist;
        u16 adx = (u16)(dx < 0 ? -dx : dx);
        u16 ady = (u16)(dy < 0 ? -dy : dy);
        if (adx > ady) adist = adx + (ady >> 1);
        else adist = ady + (adx >> 1);
        if (adist == 0) adist = 1;
        ndx = (s16)(((s32)dx * FIREBALL_SPEED) / (s32)adist);
        ndy = (s16)(((s32)dy * FIREBALL_SPEED) / (s32)adist);
    }

    p->x = sx;
    p->y = sy;
    p->dx = ndx;
    p->dy = ndy;
    p->animFrame = 0;
    p->animTimer = 0;
    p->state = PROJ_FLYING;
    p->sourceEnemy = sourceIdx;
    p->type = PROJ_TYPE_FIREBALL;
    p->angle = projAtan2(dy, dx);
}

/* Lookup table: sin/cos approximation for 8 compass directions (8.8 fixed)
 * 0=north(0,+), 1=NE(+,+), 2=east(+,0), 3=SE(+,-), etc. */
static const s16 cosTable8[8] = {  0, 181, 256, 181, 0, -181, -256, -181};
static const s16 sinTable8[8] = {256, 181,   0,-181,-256,-181,    0,  181};

void spawnRocket(u16 px, u16 py, s16 angle) {
    u8 i;
    Projectile *p = 0;
    s16 sa, ca;
    u8 dir8;

    /* Find free slot */
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (g_projectiles[i].state == PROJ_DEAD) {
            p = &g_projectiles[i];
            break;
        }
    }
    if (!p) return;

    /* Convert 10-bit angle to velocity using sin/cos.
     * VB angle 0=north, CW. 1024 steps.
     * Map to 8 directions for speed table, then refine with actual angle. */
    /* Use actual trig approximation for smooth direction */
    /* angle 0=north, 256=east, 512=south, 768=west */
    {
        /* Approximate sin/cos from the angle (0-1023):
         * dx = sin(angle) * ROCKET_SPEED / 256
         * dy = cos(angle) * ROCKET_SPEED / 256
         * We use the VB LUT approach: quarter-wave lookup */
        s16 a = angle & 1023;
        s16 sval, cval;

        /* Simple 8-direction approximation for rocket speed */
        dir8 = (u8)(((a + 64) >> 7) & 7);
        sval = cosTable8[dir8]; /* note: cos for VB coord system */
        cval = sinTable8[dir8];

        p->dx = (s16)(((s32)sval * ROCKET_SPEED) >> 8);
        p->dy = (s16)(((s32)cval * ROCKET_SPEED) >> 8);
    }

    p->x = px;
    p->y = py;
    p->animFrame = 0;
    p->animTimer = 0;
    p->state = PROJ_FLYING;
    p->sourceEnemy = 255; /* player-fired */
    p->type = PROJ_TYPE_ROCKET;
    p->angle = angle;
}

void updateProjectiles(u16 playerX, u16 playerY, s16 playerA) {
    u8 i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_projectiles[i];
        if (p->state == PROJ_DEAD) continue;

        if (p->state == PROJ_FLYING) {
            s16 nx, ny;
            u8 tx, ty;
            s16 pdx, pdy;
            u8 hitSomething = 0;

            /* Move */
            nx = (s16)p->x + p->dx;
            ny = (s16)p->y + p->dy;

            /* Wall collision */
            tx = (u8)((u16)nx >> 8);
            ty = (u8)((u16)ny >> 8);
            if (tx >= MAP_X || ty >= MAP_Y || IsWall(tx, ty)) {
                hitSomething = 1;
            }

            if (!hitSomething) {
                p->x = (u16)nx;
                p->y = (u16)ny;
            }

            /* Enemy collision for player rockets */
            if (!hitSomething && p->type == PROJ_TYPE_ROCKET) {
                u8 ei;
                for (ei = 0; ei < MAX_ENEMIES; ei++) {
                    EnemyState *e = &g_enemies[ei];
                    s16 edx, edy;
                    if (!e->active || e->state == ES_DEAD) continue;
                    edx = (s16)p->x - (s16)e->x;
                    edy = (s16)p->y - (s16)e->y;
                    if (edx < 0) edx = -edx;
                    if (edy < 0) edy = -edy;
                    if (edx < 64 && edy < 64) {
                        /* Direct hit on enemy */
                        hitSomething = 2;
                        break;
                    }
                }
            }

            /* Player collision for enemy fireballs */
            if (!hitSomething && p->type == PROJ_TYPE_FIREBALL) {
                pdx = (s16)p->x - (s16)playerX;
                pdy = (s16)p->y - (s16)playerY;
                if (pdx < 0) pdx = -pdx;
                if (pdy < 0) pdy = -pdy;
                if (pdx < 64 && pdy < 64) {
                    /* Hit player with fireball */
                    u8 damage = FIREBALL_DAMAGE_MIN +
                        (projRandom() % (FIREBALL_DAMAGE_MAX - FIREBALL_DAMAGE_MIN + 1));
                    g_lastEnemyDamage = damage;

                    /* Compute damage direction */
                    {
                        s16 atx = (s16)p->x - (s16)playerX;
                        s16 aty = (s16)p->y - (s16)playerY;
                        s16 angleToProj = (256 - projAtan2(aty, atx)) & 1023;
                        s16 relAngle = angleToProj - playerA;
                        if (relAngle > 512)  relAngle -= 1024;
                        if (relAngle < -512) relAngle += 1024;
                        if (relAngle > 128)       g_lastEnemyDamageDir =  1;
                        else if (relAngle < -128) g_lastEnemyDamageDir = -1;
                        else                      g_lastEnemyDamageDir =  0;
                    }
                    hitSomething = 1;
                }
            }

            if (hitSomething) {
                /* Explode */
                p->state = PROJ_EXPLODING;
                p->animFrame = 0;
                p->animTimer = 0;

                if (p->type == PROJ_TYPE_ROCKET) {
                    /* Rocket explosion: splash damage to all enemies and player */
                    u8 ei;
                    playPlayerSFX(SFX_BARREL_EXPLODE);

                    /* Damage enemies in splash radius */
                    for (ei = 0; ei < MAX_ENEMIES; ei++) {
                        EnemyState *e = &g_enemies[ei];
                        s16 edx, edy;
                        u16 dist;
                        if (!e->active || e->state == ES_DEAD) continue;
                        edx = (s16)p->x - (s16)e->x;
                        edy = (s16)p->y - (s16)e->y;
                        dist = approxDist(edx, edy);
                        if (dist < ROCKET_SPLASH_RADIUS) {
                            u8 dmg;
                            if (hitSomething == 2) {
                                /* Direct hit: full damage */
                                dmg = ROCKET_DIRECT_DAMAGE;
                            } else {
                                /* Splash: damage falls off with distance */
                                dmg = (u8)((u16)(ROCKET_SPLASH_RADIUS - dist) >> 2);
                                if (dmg > ROCKET_DIRECT_DAMAGE) dmg = ROCKET_DIRECT_DAMAGE;
                            }
                            if (dmg > 0) {
                                applyDamageToEnemy(e, dmg, playerX, playerY);
                            }
                        }
                    }

                    /* Player self-damage from own rocket splash */
                    {
                        s16 ppdx = (s16)p->x - (s16)playerX;
                        s16 ppdy = (s16)p->y - (s16)playerY;
                        u16 pdist = approxDist(ppdx, ppdy);
                        if (pdist < ROCKET_SPLASH_RADIUS) {
                            u8 selfDmg = (u8)((u16)(ROCKET_SPLASH_RADIUS - pdist) >> 2);
                            if (selfDmg > 0) {
                                g_lastEnemyDamage = selfDmg;
                                g_lastEnemyDamageDir = 0;
                            }
                        }
                    }
                } else {
                    playPlayerSFX(SFX_PROJECTILE_CONTACT);
                }
                continue;
            }

            /* Flight animation (2-frame cycle) */
            p->animTimer++;
            if (p->animTimer >= 4) {
                p->animTimer = 0;
                p->animFrame ^= 1;
            }
        }
        else if (p->state == PROJ_EXPLODING) {
            /* Explosion animation */
            p->animTimer++;
            if (p->animTimer >= FIREBALL_EXPLODE_RATE) {
                p->animTimer = 0;
                p->animFrame++;
                if (p->animFrame >= FIREBALL_EXPLODE_NFRAMES) {
                    p->state = PROJ_DEAD;
                }
            }
        }
    }
}
