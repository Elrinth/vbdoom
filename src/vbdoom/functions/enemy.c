#include <libgccvb.h>
#include <mem.h>
#include "enemy.h"
#include "pickup.h"
#include "projectile.h"
#include "RayCasterFixed.h"
#include "RayCaster.h"
#include "doomgfx.h"
#include "sndplay.h"
#include "../assets/audio/doom_sfx.h"

/* Global enemy array */
EnemyState g_enemies[MAX_ENEMIES];

/* Frame counter from game loop -- used for distant enemy AI throttling */
extern u16 g_levelFrames;

/* Distance threshold for AI throttling: enemies farther than this
 * only get full AI updates every 4th frame.  ~8 map tiles squared. */
#define ENEMY_FAR_DIST_SQ  2000000

/* Visible enemy indices (closest 5, sorted by distance) */
u8 g_visibleEnemies[MAX_VISIBLE_ENEMIES] = {255, 255, 255, 255, 255};
u8 g_numVisibleEnemies = 0;

/* Damage feedback globals (read+cleared by game loop each frame) */
u8 g_lastEnemyDamage = 0;
s8 g_lastEnemyDamageDir = 0;

/* Kill tracking (for level stats screen) */
u8 g_enemiesKilled = 0;
u8 g_totalEnemies = 0;

/* Sound event globals -- kept for backward compatibility but no longer
 * read by sndplay.c. Direct playEnemySFX() calls are used instead. */
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

/* Fast modulo for small unsigned values (0-255), avoids V810 division.
 * Uses multiply-shift to compute quotient, then subtracts.
 * NOTE: GCC statement expressions ensure argument is evaluated only once
 *       (safe to pass function calls like enemyRandom()). */
#define FAST_MOD3(n)  ({ u8 _v=(n); (u8)(_v - ((u16)((u16)_v * 171u) >> 9) * 3u); })
#define FAST_MOD5(n)  ({ u8 _v=(n); (u8)(_v - ((u16)((u16)_v * 205u) >> 10) * 5u); })
#define FAST_MOD8(n)  ((u8)((n) & 7u))
#define FAST_MOD10(n) ({ u8 _v=(n); (u8)(_v - ((u16)((u16)_v * 205u) >> 11) * 10u); })
#define FAST_MOD13(n) ({ u8 _v=(n); (u8)(_v - ((u16)(((u32)_v * 316u) >> 12)) * 13u); })

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

/* Fixed-point atan2 (0-1023 range).
 * Optimized: shift-normalize both components so the larger fits in 8 bits,
 * then the division is u16 / u8 instead of the original s32 / s32. */
/* Reciprocal LUT for fixedAtan2: g_recip8[i] = 32768/i for i=1..255 (512 bytes ROM).
 * Replaces (minor<<7)/major with (minor * g_recip8[major]) >> 8. */
static const u16 g_recip8[256] = {
        0,32768,16384,10922, 8192, 6553, 5461, 4681, 4096, 3640, 3276, 2978, 2730, 2520, 2340, 2184,
     2048, 1927, 1820, 1724, 1638, 1560, 1489, 1424, 1365, 1310, 1260, 1213, 1170, 1129, 1092, 1057,
     1024,  992,  963,  936,  910,  885,  862,  840,  819,  799,  780,  762,  744,  728,  712,  697,
      682,  668,  655,  642,  630,  618,  606,  595,  585,  574,  564,  555,  546,  537,  528,  520,
      512,  504,  496,  489,  481,  474,  468,  461,  455,  448,  442,  436,  431,  425,  420,  414,
      409,  404,  399,  394,  390,  385,  381,  376,  372,  368,  364,  360,  356,  352,  348,  344,
      341,  337,  334,  330,  327,  324,  321,  318,  315,  312,  309,  306,  303,  300,  297,  295,
      292,  289,  287,  284,  282,  280,  277,  275,  273,  270,  268,  266,  264,  262,  260,  258,
      256,  254,  252,  250,  248,  246,  244,  242,  240,  239,  237,  235,  234,  232,  230,  229,
      227,  225,  224,  222,  221,  219,  218,  217,  215,  214,  212,  211,  210,  208,  207,  206,
      204,  203,  202,  201,  199,  198,  197,  196,  195,  193,  192,  191,  190,  189,  188,  187,
      186,  185,  184,  183,  182,  181,  180,  179,  178,  177,  176,  175,  174,  173,  172,  171,
      170,  169,  168,  168,  167,  166,  165,  164,  163,  163,  162,  161,  160,  159,  159,  158,
      157,  156,  156,  155,  154,  153,  153,  152,  151,  151,  150,  149,  148,  148,  147,  146,
      146,  145,  144,  144,  143,  143,  142,  141,  141,  140,  140,  139,  138,  138,  137,  137,
      136,  135,  135,  134,  134,  133,  133,  132,  132,  131,  131,  130,  130,  129,  129,  128
};

static s16 fixedAtan2(s16 dy, s16 dx) {
    s16 angle;
    u16 adx = (u16)(dx < 0 ? -dx : dx);
    u16 ady = (u16)(dy < 0 ? -dy : dy);
    u16 major, minor;

    if (adx == 0 && ady == 0) return 0;

    if (adx >= ady) { major = adx; minor = ady; }
    else            { major = ady; minor = adx; }

    /* Normalize: shift both right until major fits in 8 bits (max 255).
     * This preserves the ratio minor/major while making the multiply cheap. */
    while (major > 255) { major >>= 1; minor >>= 1; }

    /* Compute ratio = minor * 128 / major via reciprocal LUT (division-free) */
    if (major == 0)
        angle = (adx >= ady) ? 0 : 256;
    else if (adx >= ady)
        angle = (s16)(((u32)minor * g_recip8[major]) >> 8);
    else
        angle = 256 - (s16)(((u32)minor * g_recip8[major]) >> 8);

    /* Quadrant adjustment */
    if (dx >= 0 && dy < 0) {
        angle = 1024 - angle;
    } else if (dx < 0 && dy < 0) {
        angle = 512 + angle;
    } else if (dx < 0 && dy >= 0) {
        angle = 512 - angle;
    }

    return angle & 1023;
}

/* ---- Distance helper (returns sqrt-ish approx in fixed-point units) ---- */
u16 approxDist(s16 dx, s16 dy) {
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

/* hasLineOfSight() now provided by RayCasterFixed.c (Bresenham tile-walk) */

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

    /* Increase effective distance so enemies fire less often overall.
     * This adds a base "reluctance" -- even close enemies only fire ~60% of frames. */
    dist += 100;
    if (dist > 255) dist = 255;

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

    /* Enemy 0: zombieman, main hall south */
    g_enemies[0].x = 15 * 256 + 128;
    g_enemies[0].y = 21 * 256 + 128;
    g_enemies[0].active = true;
    g_enemies[0].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[0].health = ZOMBIE_HEALTH;

    /* Enemy 1: zombieman, main hall east */
    g_enemies[1].x = 17 * 256 + 128;
    g_enemies[1].y = 23 * 256 + 128;
    g_enemies[1].active = true;
    g_enemies[1].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[1].health = ZOMBIE_HEALTH;

    /* Enemy 2: sergeant (shotgun guy), zigzag room */
    g_enemies[2].x = 8 * 256 + 128;
    g_enemies[2].y = 14 * 256 + 128;
    g_enemies[2].active = true;
    g_enemies[2].enemyType = ETYPE_SERGEANT;
    g_enemies[2].health = SGT_HEALTH;

    /* Enemy 3: zombieman, zigzag room east */
    g_enemies[3].x = 18 * 256 + 128;
    g_enemies[3].y = 16 * 256 + 128;
    g_enemies[3].active = true;
    g_enemies[3].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[3].health = ZOMBIE_HEALTH;

    /* Enemy 4: IMP, zigzag room center */
    g_enemies[4].x = 12 * 256 + 128;
    g_enemies[4].y = 15 * 256 + 128;
    g_enemies[4].active = true;
    g_enemies[4].enemyType = ETYPE_IMP;
    g_enemies[4].health = IMP_HEALTH;

    /* Enemy 5: IMP, computer room */
    g_enemies[5].x = 15 * 256 + 128;
    g_enemies[5].y = 7 * 256 + 128;
    g_enemies[5].active = true;
    g_enemies[5].enemyType = ETYPE_IMP;
    g_enemies[5].health = IMP_HEALTH;

    /* Enemy 6: zombieman, east side room */
    g_enemies[6].x = 27 * 256 + 128;
    g_enemies[6].y = 14 * 256 + 128;
    g_enemies[6].active = true;
    g_enemies[6].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[6].health = ZOMBIE_HEALTH;

    /* Enemy 7: sergeant, near north door */
    g_enemies[7].x = 15 * 256 + 128;
    g_enemies[7].y = 12 * 256 + 128;
    g_enemies[7].active = true;
    g_enemies[7].enemyType = ETYPE_SERGEANT;
    g_enemies[7].health = SGT_HEALTH;

    /* Enemy 8: IMP, exit room guard */
    g_enemies[8].x = 26 * 256 + 128;
    g_enemies[8].y = 3 * 256 + 128;
    g_enemies[8].active = true;
    g_enemies[8].enemyType = ETYPE_IMP;
    g_enemies[8].health = IMP_HEALTH;

    /* Enemy 9: zombieman, west side room */
    g_enemies[9].x = 4 * 256 + 128;
    g_enemies[9].y = 22 * 256 + 128;
    g_enemies[9].active = true;
    g_enemies[9].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[9].health = ZOMBIE_HEALTH;

    /* Enemy 10: Demon (Pinky), main hall center */
    g_enemies[10].x = 14 * 256 + 128;
    g_enemies[10].y = 18 * 256 + 128;
    g_enemies[10].active = true;
    g_enemies[10].enemyType = ETYPE_DEMON;
    g_enemies[10].health = DEMON_HEALTH;

    /* Enemy 11: Demon (Pinky), computer room south */
    g_enemies[11].x = 16 * 256 + 128;
    g_enemies[11].y = 10 * 256 + 128;
    g_enemies[11].active = true;
    g_enemies[11].enemyType = ETYPE_DEMON;
    g_enemies[11].health = DEMON_HEALTH;

    /* Enemy 12: Demon (Pinky), east corridor */
    g_enemies[12].x = 24 * 256 + 128;
    g_enemies[12].y = 8 * 256 + 128;
    g_enemies[12].active = true;
    g_enemies[12].enemyType = ETYPE_DEMON;
    g_enemies[12].health = DEMON_HEALTH;

    /* Enemy 13: Commando (Chaingunner), exit room */
    g_enemies[13].x = 25 * 256 + 128;
    g_enemies[13].y = 2 * 256 + 128;
    g_enemies[13].active = true;
    g_enemies[13].enemyType = ETYPE_COMMANDO;
    g_enemies[13].health = COMMANDO_HEALTH;
}

void initEnemiesE1M2(void) {
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

    /* E1M2: Wolfenstein bunker -- guards patrol corridors */

    /* Guard 0: zombieman, south corridor west */
    g_enemies[0].x = 5 * 256 + 128;
    g_enemies[0].y = 21 * 256 + 128;
    g_enemies[0].active = true;
    g_enemies[0].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[0].health = ZOMBIE_HEALTH;

    /* Guard 1: zombieman, south corridor east */
    g_enemies[1].x = 26 * 256 + 128;
    g_enemies[1].y = 21 * 256 + 128;
    g_enemies[1].active = true;
    g_enemies[1].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[1].health = ZOMBIE_HEALTH;

    /* Guard 2: zombieman, central command room */
    g_enemies[2].x = 12 * 256 + 128;
    g_enemies[2].y = 10 * 256 + 128;
    g_enemies[2].active = true;
    g_enemies[2].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[2].health = ZOMBIE_HEALTH;

    /* Guard 3: zombieman, central room east */
    g_enemies[3].x = 19 * 256 + 128;
    g_enemies[3].y = 11 * 256 + 128;
    g_enemies[3].active = true;
    g_enemies[3].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[3].health = ZOMBIE_HEALTH;

    /* Guard 4: sergeant, officer quarters NW */
    g_enemies[4].x = 5 * 256 + 128;
    g_enemies[4].y = 3 * 256 + 128;
    g_enemies[4].active = true;
    g_enemies[4].enemyType = ETYPE_SERGEANT;
    g_enemies[4].health = SGT_HEALTH;

    /* Guard 5: sergeant, north corridor */
    g_enemies[5].x = 15 * 256 + 128;
    g_enemies[5].y = 8 * 256 + 128;
    g_enemies[5].active = true;
    g_enemies[5].enemyType = ETYPE_SERGEANT;
    g_enemies[5].health = SGT_HEALTH;

    /* Guard 6: IMP, exit room guard */
    g_enemies[6].x = 26 * 256 + 128;
    g_enemies[6].y = 4 * 256 + 128;
    g_enemies[6].active = true;
    g_enemies[6].enemyType = ETYPE_IMP;
    g_enemies[6].health = IMP_HEALTH;

    /* Guard 7: zombieman, west barracks */
    g_enemies[7].x = 4 * 256 + 128;
    g_enemies[7].y = 16 * 256 + 128;
    g_enemies[7].active = true;
    g_enemies[7].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[7].health = ZOMBIE_HEALTH;

    /* Guard 8: zombieman, east armory */
    g_enemies[8].x = 28 * 256 + 128;
    g_enemies[8].y = 16 * 256 + 128;
    g_enemies[8].active = true;
    g_enemies[8].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[8].health = ZOMBIE_HEALTH;

    /* Guard 9: Demon, central room south */
    g_enemies[9].x = 15 * 256 + 128;
    g_enemies[9].y = 14 * 256 + 128;
    g_enemies[9].active = true;
    g_enemies[9].enemyType = ETYPE_DEMON;
    g_enemies[9].health = DEMON_HEALTH;

    /* Guard 10: zombieman, long corridor center */
    g_enemies[10].x = 15 * 256 + 128;
    g_enemies[10].y = 24 * 256 + 128;
    g_enemies[10].active = true;
    g_enemies[10].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[10].health = ZOMBIE_HEALTH;

    /* Guard 11: IMP, north corridor east */
    g_enemies[11].x = 20 * 256 + 128;
    g_enemies[11].y = 9 * 256 + 128;
    g_enemies[11].active = true;
    g_enemies[11].enemyType = ETYPE_IMP;
    g_enemies[11].health = IMP_HEALTH;
}

void initEnemiesE1M3(void) {
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

    /* E1M3: Hell-touched research facility -- heavy combat */

    /* 0: zombieman, south corridor west */
    g_enemies[0].x = 5 * 256 + 128;
    g_enemies[0].y = 23 * 256 + 128;
    g_enemies[0].active = true;
    g_enemies[0].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[0].health = ZOMBIE_HEALTH;

    /* 1: zombieman, south corridor east */
    g_enemies[1].x = 20 * 256 + 128;
    g_enemies[1].y = 24 * 256 + 128;
    g_enemies[1].active = true;
    g_enemies[1].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[1].health = ZOMBIE_HEALTH;

    /* 2: zombieman, central arena south */
    g_enemies[2].x = 8 * 256 + 128;
    g_enemies[2].y = 12 * 256 + 128;
    g_enemies[2].active = true;
    g_enemies[2].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[2].health = ZOMBIE_HEALTH;

    /* 3: zombieman, east wing lab */
    g_enemies[3].x = 28 * 256 + 128;
    g_enemies[3].y = 16 * 256 + 128;
    g_enemies[3].active = true;
    g_enemies[3].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[3].health = ZOMBIE_HEALTH;

    /* 4: sergeant, central arena north */
    g_enemies[4].x = 15 * 256 + 128;
    g_enemies[4].y = 9 * 256 + 128;
    g_enemies[4].active = true;
    g_enemies[4].enemyType = ETYPE_SERGEANT;
    g_enemies[4].health = SGT_HEALTH;

    /* 5: sergeant, west wing */
    g_enemies[5].x = 3 * 256 + 128;
    g_enemies[5].y = 17 * 256 + 128;
    g_enemies[5].active = true;
    g_enemies[5].enemyType = ETYPE_SERGEANT;
    g_enemies[5].health = SGT_HEALTH;

    /* 6: sergeant, east wing */
    g_enemies[6].x = 28 * 256 + 128;
    g_enemies[6].y = 18 * 256 + 128;
    g_enemies[6].active = true;
    g_enemies[6].enemyType = ETYPE_SERGEANT;
    g_enemies[6].health = SGT_HEALTH;

    /* 7: imp, central arena east */
    g_enemies[7].x = 22 * 256 + 128;
    g_enemies[7].y = 10 * 256 + 128;
    g_enemies[7].active = true;
    g_enemies[7].enemyType = ETYPE_IMP;
    g_enemies[7].health = IMP_HEALTH;

    /* 8: imp, south corridor center */
    g_enemies[8].x = 15 * 256 + 128;
    g_enemies[8].y = 17 * 256 + 128;
    g_enemies[8].active = true;
    g_enemies[8].enemyType = ETYPE_IMP;
    g_enemies[8].health = IMP_HEALTH;

    /* 9: imp, command center */
    g_enemies[9].x = 16 * 256 + 128;
    g_enemies[9].y = 4 * 256 + 128;
    g_enemies[9].active = true;
    g_enemies[9].enemyType = ETYPE_IMP;
    g_enemies[9].health = IMP_HEALTH;

    /* 10: demon, central arena center */
    g_enemies[10].x = 15 * 256 + 128;
    g_enemies[10].y = 11 * 256 + 128;
    g_enemies[10].active = true;
    g_enemies[10].enemyType = ETYPE_DEMON;
    g_enemies[10].health = DEMON_HEALTH;

    /* 11: demon, west wing storage */
    g_enemies[11].x = 4 * 256 + 128;
    g_enemies[11].y = 15 * 256 + 128;
    g_enemies[11].active = true;
    g_enemies[11].enemyType = ETYPE_DEMON;
    g_enemies[11].health = DEMON_HEALTH;

    /* 12: demon, command center guard */
    g_enemies[12].x = 14 * 256 + 128;
    g_enemies[12].y = 6 * 256 + 128;
    g_enemies[12].active = true;
    g_enemies[12].enemyType = ETYPE_DEMON;
    g_enemies[12].health = DEMON_HEALTH;
}

void initEnemiesE1M4(void) {
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

    /* E1M4: web-editor export */
    g_enemies[0].x = 23 * 256 + 128;
    g_enemies[0].y = 29 * 256 + 128;
    g_enemies[0].angle = 768;
    g_enemies[0].active = true;
    g_enemies[0].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[0].health = ZOMBIE_HEALTH;
    g_enemies[1].x = 20 * 256 + 128;
    g_enemies[1].y = 29 * 256 + 128;
    g_enemies[1].angle = 768;
    g_enemies[1].active = true;
    g_enemies[1].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[1].health = ZOMBIE_HEALTH;
    g_enemies[2].x = 30 * 256 + 128;
    g_enemies[2].y = 19 * 256 + 128;
    g_enemies[2].angle = 0;
    g_enemies[2].active = true;
    g_enemies[2].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[2].health = ZOMBIE_HEALTH;
    g_enemies[3].x = 28 * 256 + 128;
    g_enemies[3].y = 19 * 256 + 128;
    g_enemies[3].angle = 0;
    g_enemies[3].active = true;
    g_enemies[3].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[3].health = ZOMBIE_HEALTH;
    g_enemies[4].x = 30 * 256 + 128;
    g_enemies[4].y = 21 * 256 + 128;
    g_enemies[4].angle = 0;
    g_enemies[4].active = true;
    g_enemies[4].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[4].health = ZOMBIE_HEALTH;
    g_enemies[5].x = 26 * 256 + 128;
    g_enemies[5].y = 10 * 256 + 128;
    g_enemies[5].angle = 512;
    g_enemies[5].active = true;
    g_enemies[5].enemyType = ETYPE_IMP;
    g_enemies[5].health = IMP_HEALTH;
    g_enemies[6].x = 29 * 256 + 128;
    g_enemies[6].y = 10 * 256 + 128;
    g_enemies[6].angle = 512;
    g_enemies[6].active = true;
    g_enemies[6].enemyType = ETYPE_IMP;
    g_enemies[6].health = IMP_HEALTH;
    g_enemies[7].x = 15 * 256 + 128;
    g_enemies[7].y = 25 * 256 + 128;
    g_enemies[7].angle = 0;
    g_enemies[7].active = true;
    g_enemies[7].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[7].health = ZOMBIE_HEALTH;
    g_enemies[8].x = 23 * 256 + 128;
    g_enemies[8].y = 19 * 256 + 128;
    g_enemies[8].angle = 256;
    g_enemies[8].active = true;
    g_enemies[8].enemyType = ETYPE_DEMON;
    g_enemies[8].health = DEMON_HEALTH;
    g_enemies[9].x = 5 * 256 + 128;
    g_enemies[9].y = 26 * 256 + 128;
    g_enemies[9].angle = 0;
    g_enemies[9].active = true;
    g_enemies[9].enemyType = ETYPE_DEMON;
    g_enemies[9].health = DEMON_HEALTH;
    g_enemies[10].x = 2 * 256 + 128;
    g_enemies[10].y = 30 * 256 + 128;
    g_enemies[10].angle = 0;
    g_enemies[10].active = true;
    g_enemies[10].enemyType = ETYPE_SERGEANT;
    g_enemies[10].health = SGT_HEALTH;
    g_enemies[11].x = 4 * 256 + 128;
    g_enemies[11].y = 30 * 256 + 128;
    g_enemies[11].angle = 0;
    g_enemies[11].active = true;
    g_enemies[11].enemyType = ETYPE_IMP;
    g_enemies[11].health = IMP_HEALTH;
    g_enemies[12].x = 3 * 256 + 128;
    g_enemies[12].y = 12 * 256 + 128;
    g_enemies[12].angle = 0;
    g_enemies[12].active = true;
    g_enemies[12].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[12].health = ZOMBIE_HEALTH;
    g_enemies[13].x = 4 * 256 + 128;
    g_enemies[13].y = 11 * 256 + 128;
    g_enemies[13].angle = 0;
    g_enemies[13].active = true;
    g_enemies[13].enemyType = ETYPE_SERGEANT;
    g_enemies[13].health = SGT_HEALTH;
    g_enemies[14].x = 7 * 256 + 128;
    g_enemies[14].y = 7 * 256 + 128;
    g_enemies[14].angle = 0;
    g_enemies[14].active = true;
    g_enemies[14].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[14].health = ZOMBIE_HEALTH;
    g_enemies[15].x = 21 * 256 + 128;
    g_enemies[15].y = 11 * 256 + 128;
    g_enemies[15].angle = 0;
    g_enemies[15].active = true;
    g_enemies[15].enemyType = ETYPE_IMP;
    g_enemies[15].health = IMP_HEALTH;
    g_enemies[16].x = 22 * 256 + 128;
    g_enemies[16].y = 8 * 256 + 128;
    g_enemies[16].angle = 0;
    g_enemies[16].active = true;
    g_enemies[16].enemyType = ETYPE_DEMON;
    g_enemies[16].health = DEMON_HEALTH;
    g_enemies[17].x = 18 * 256 + 128;
    g_enemies[17].y = 6 * 256 + 128;
    g_enemies[17].angle = 0;
    g_enemies[17].active = true;
    g_enemies[17].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[17].health = ZOMBIE_HEALTH;
    g_enemies[18].x = 29 * 256 + 128;
    g_enemies[18].y = 2 * 256 + 128;
    g_enemies[18].angle = 0;
    g_enemies[18].active = true;
    g_enemies[18].enemyType = ETYPE_SERGEANT;
    g_enemies[18].health = SGT_HEALTH;
    g_enemies[19].x = 9 * 256 + 128;
    g_enemies[19].y = 16 * 256 + 128;
    g_enemies[19].angle = 0;
    g_enemies[19].active = true;
    g_enemies[19].enemyType = ETYPE_ZOMBIEMAN;
    g_enemies[19].health = ZOMBIE_HEALTH;
    g_enemies[20].x = 1 * 256 + 128;
    g_enemies[20].y = 2 * 256 + 128;
    g_enemies[20].angle = 0;
    g_enemies[20].active = true;
    g_enemies[20].enemyType = ETYPE_IMP;
    g_enemies[20].health = IMP_HEALTH;
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
bool applyDamageToEnemy(EnemyState *e, u8 damage, u16 playerX, u16 playerY) {
    if (e->health <= damage) {
        /* Kill enemy */
        e->health = 0;
        e->state = ES_DEAD;
        e->animFrame = 0;
        e->animTimer = 0;
        g_enemiesKilled++;

        /* Drop item based on enemy type (IMP drops nothing, like Doom) */
        if (e->enemyType == ETYPE_SERGEANT) {
            spawnPickup(PICKUP_WEAPON_SHOTGUN, e->x, e->y);
        } else if (e->enemyType == ETYPE_COMMANDO) {
            spawnPickup(PICKUP_WEAPON_CHAINGUN, e->x, e->y);
        } else if (e->enemyType == ETYPE_ZOMBIEMAN) {
            spawnPickup(PICKUP_AMMO_CLIP, e->x, e->y);
        }

        /* Death sound event -- PCM, type-specific variant */
        {
            s16 ddx = (s16)e->x - (s16)playerX;
            s16 ddy = (s16)e->y - (s16)playerY;
            u16 d = approxDist(ddx, ddy);
            u8 sdist = (u8)(d > 4080 ? 255 : d >> 4);
            if (sdist == 0) sdist = 1;
            if (e->enemyType == ETYPE_IMP)
                playEnemySFX(SFX_IMP_DEATH1, sdist);
            else if (e->enemyType == ETYPE_DEMON)
                playEnemySFX(SFX_PINKY_DEATH, sdist);
            else
                playEnemySFX(SFX_POSSESSED_DEATH1, sdist);
        }
        return true;
    } else {
        u8 painchance;
        e->health -= damage;
        /* Pain chance check (Doom-style) */
        painchance = ZOMBIE_PAINCHANCE;
        if (e->enemyType == ETYPE_SERGEANT) painchance = SGT_PAINCHANCE;
        else if (e->enemyType == ETYPE_IMP) painchance = IMP_PAINCHANCE;
        else if (e->enemyType == ETYPE_DEMON) painchance = DEMON_PAINCHANCE;
        else if (e->enemyType == ETYPE_COMMANDO) painchance = COMMANDO_PAINCHANCE;
        if (enemyRandom() < painchance) {
            e->state = ES_PAIN;
            e->stateTimer = 0;
            /* Pain sound -- PCM (IMP/Demon reuse possessed pain) */
            {
                s16 ddx = (s16)e->x - (s16)playerX;
                s16 ddy = (s16)e->y - (s16)playerY;
                u16 d = approxDist(ddx, ddy);
                u8 sdist = (u8)(d > 4080 ? 255 : d >> 4);
                if (sdist == 0) sdist = 1;
                playEnemySFX(SFX_POSSESSED_PAIN, sdist);
            }
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
        if (dist > HITSCAN_RANGE_DIST) continue;

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
                /* Doom P_GunShot: 5*(P_Random()%3 + 1) = 5,10,15 */
                pelletDmg[target] += 5 * (FAST_MOD3(enemyRandom()) + 1);
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
        /* ---- Pistol / Chaingun / Fist: single hitscan ---- */
        /* Fist gets wider cone (64) for easier melee hits; pistol/chaingun use 32 */
        s16 cone = (weaponType == 1) ? 64 : 32;
        u8 target = hitscanFindEnemy(playerX, playerY, playerA & 1023, cone);

        if (target != 255) {
            /* Fist: enforce melee range -- reject if enemy is too far */
            if (weaponType == 1) {
                s16 dx = (s16)g_enemies[target].x - (s16)playerX;
                s16 dy = (s16)g_enemies[target].y - (s16)playerY;
                s32 dist = ((s32)dx * dx + (s32)dy * dy) >> 8;
                if (dist > FIST_MELEE_RANGE) target = 255;
            }
        }
        if (target != 255) {
            u8 damage;
            if (weaponType == 1) {
                /* Fist: (P_Random()%10 + 1) * 2 = 2-20 */
                damage = (FAST_MOD10(enemyRandom()) + 1) * 2;
            } else {
                /* Doom P_GunShot: 5*(P_Random()%3 + 1) = 5,10,15 */
                damage = 5 * (FAST_MOD3(enemyRandom()) + 1);
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

    /* IMPs use impAttack() instead of this function.
     * Commando fires 2 rapid shots per attack frame, sergeant 3 pellets. */
    pellets = (e->enemyType == ETYPE_SERGEANT) ? 3 :
              (e->enemyType == ETYPE_COMMANDO) ? 2 : 1;

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
                /* Doom A_PosAttack: ((P_Random()%5)+1)*3 = 3,6,9,12,15 */
                totalDamage += (FAST_MOD5(enemyRandom()) + 1) * 3;
            }
        }
    }

    if (totalDamage > 0) {
        g_lastEnemyDamage += totalDamage;
        if (g_lastEnemyDamage > 50) g_lastEnemyDamage = 50;  /* per-frame cap */

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

    /* Shoot sound event -- each hitscan enemy uses its weapon sound */
    {
        u8 sdist = (u8)(dist > 4080 ? 255 : dist >> 4);
        if (sdist == 0) sdist = 1;
        if (e->enemyType == ETYPE_COMMANDO)
            playEnemySFX(SFX_SHOTGUN, sdist);
        else if (e->enemyType == ETYPE_SERGEANT)
            playEnemySFX(SFX_SHOTGUN, sdist);
        else
            playEnemySFX(SFX_PISTOL, sdist);  /* zombieman */
    }
}

/*
 * IMP attacks: melee claw at close range, fireball projectile at distance.
 * Claw: hitscan, 3-24 damage (like Doom: (P_Random()%8 + 1) * 3)
 * Fireball: spawns a Projectile that travels toward the player.
 */
static void impAttack(EnemyState *e, u8 idx, u16 playerX, u16 playerY, s16 playerA) {
    s16 dx = (s16)playerX - (s16)e->x;
    s16 dy = (s16)playerY - (s16)e->y;
    u16 dist = approxDist(dx, dy);
    u8 sdist = (u8)(dist > 4080 ? 255 : dist >> 4);
    if (sdist == 0) sdist = 1;

    if (dist < IMP_MELEE_DIST * 2) {
        /* Melee claw attack */
        u8 damage = (FAST_MOD8(enemyRandom()) + 1) * 3;  /* Doom A_TroopAttack: 3-24 */
        if (hasLineOfSight(e->x, e->y, playerX, playerY)) {
            g_lastEnemyDamage += damage;
            if (g_lastEnemyDamage > 50) g_lastEnemyDamage = 50;  /* per-frame cap */
            /* Compute damage direction */
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
        playEnemySFX(SFX_CLAW_ATTACK, sdist);
    } else {
        /* Ranged fireball */
        spawnFireball(e->x, e->y, playerX, playerY, idx);
        playEnemySFX(SFX_IMP_ACTIVITY, sdist);
    }
}

/*
 * Demon (Pinky) attack: melee-only, claw/bite.
 * Doom damage: (P_Random()%10 + 1) * 4 = 4-40
 */
static void demonAttack(EnemyState *e, u8 idx, u16 playerX, u16 playerY, s16 playerA) {
    s16 dx = (s16)playerX - (s16)e->x;
    s16 dy = (s16)playerY - (s16)e->y;
    u16 dist = approxDist(dx, dy);
    u8 sdist = (u8)(dist > 4080 ? 255 : dist >> 4);
    if (sdist == 0) sdist = 1;

    /* Only melee -- check close range */
    if (dist < DEMON_MELEE_DIST * 2) {
        u8 damage = (FAST_MOD10(enemyRandom()) + 1) * 4;  /* Doom A_SargAttack: 4-40 */
        if (hasLineOfSight(e->x, e->y, playerX, playerY)) {
            g_lastEnemyDamage += damage;
            if (g_lastEnemyDamage > 50) g_lastEnemyDamage = 50;  /* per-frame cap */
            /* Compute damage direction */
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
        playEnemySFX(SFX_PINKY_ATTACK, sdist);
    }
    /* No ranged attack -- demon must get close */
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

        /* Throttle distant WALK-state enemies only.
         * Keep ATTACK/reaction states full-rate so close combat remains responsive. */
        if (dist > ENEMY_FAR_DIST_SQ && (g_levelFrames & 3) != 0
            && e->state == ES_WALK) {
            e->animTimer++;
            if (e->stateTimer > 0) e->stateTimer++;
            continue;
        }

        switch (e->state) {
        case ES_IDLE: {
            if (dist < (s32)ENEMY_SIGHT_DIST) {
                e->state = ES_WALK;
                e->animFrame = 0;
                e->animTimer = 0;
                e->movedir = DI_NODIR;
                e->movecount = 0;
                e->angle = fixedAtan2(dy, dx);
                /* Aggro sound -- PCM, type-specific sight variant */
                {
                    u16 d = approxDist(dx, dy);
                    u8 sdist = (u8)(d > 4080 ? 255 : d >> 4);
                    if (sdist == 0) sdist = 1;
                    if (e->enemyType == ETYPE_IMP)
                        playEnemySFX(SFX_IMP_SIGHT1, sdist);
                    else if (e->enemyType == ETYPE_DEMON)
                        playEnemySFX(SFX_PINKY_SIGHT, sdist);
                    else
                        playEnemySFX(SFX_POSSESSED_SIGHT1, sdist);
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

            /* Check if we should attack (Doom-style ranged check).
             * Demons are melee-only: use tighter range so they close the gap first. */
            {
                s32 attackDist = (e->enemyType == ETYPE_DEMON)
                    ? (s32)DEMON_ATTACK_ENTER : (s32)ENEMY_ATTACK_DIST;
                if (dist < attackDist &&
                    checkMissileRange(e, playerX, playerY)) {
                    e->state = ES_ATTACK;
                    e->stateTimer = 0;
                    e->animFrame = 0;
                    if (e->enemyType == ETYPE_COMMANDO)
                        e->movecount = 0; /* reset burst counter */
                }
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

            if (e->enemyType == ETYPE_IMP || e->enemyType == ETYPE_DEMON) {
                /* 3-frame attack: cycle frames evenly across ENEMY_SHOOT_RATE,
                 * fire on the last frame, then return to walk. */
                u8 maxFrames = (e->enemyType == ETYPE_IMP) ?
                    IMP_ATTACK_ANIM_FRAMES : DEMON_ATTACK_ANIM_FRAMES;
                /* Both IMP and DEMON have 3 attack frames; 20/3=6, precomputed */
                #define ATTACK_FRAME_LEN (ENEMY_SHOOT_RATE / 3)  /* =6, compiler-folded */
                u8 frameLen = ATTACK_FRAME_LEN;

                if (e->stateTimer >= frameLen) {
                    e->stateTimer = 0;
                    e->animFrame++;

                    /* Fire on the last animation frame */
                    if (e->animFrame == maxFrames - 1) {
                        if (e->enemyType == ETYPE_IMP)
                            impAttack(e, i, playerX, playerY, playerA);
                        else
                            demonAttack(e, i, playerX, playerY, playerA);
                    }

                    /* After last frame completes, return to walk */
                    if (e->animFrame >= maxFrames) {
                        e->state = ES_WALK;
                        e->animTimer = 0;
                        e->animFrame = 0;
                        e->movecount = 4 + (enemyRandom() & 7);
                    }
                }
            } else if (e->enemyType == ETYPE_COMMANDO) {
                /* Commando (Chaingunner): rapid-fire burst -- fires every
                 * animation frame for 3 cycles (6 shots total) with a
                 * fast frame rate, then returns to walk. */
                #define COMMANDO_BURST_CYCLES 3  /* number of 2-frame loops */
                u8 frameLen = ENEMY_SHOOT_RATE / 6; /* much faster frames */
                if (frameLen < 2) frameLen = 2;

                if (e->stateTimer >= frameLen) {
                    e->stateTimer = 0;
                    e->animFrame++;

                    /* Fire on EVERY frame of the burst */
                    enemyFireAtPlayer(e, playerX, playerY, playerA);

                    if (e->animFrame >= COMMANDO_ATTACK_ANIM_FRAMES) {
                        e->animFrame = 0;
                        e->movecount++;
                        /* After enough cycles, return to walk */
                        if (e->movecount >= COMMANDO_BURST_CYCLES) {
                            e->state = ES_WALK;
                            e->animTimer = 0;
                            e->animFrame = 0;
                            e->movecount = 4 + (enemyRandom() & 7);
                        }
                    }
                }
            } else {
                /* Zombieman/Sergeant: 2-frame attack (aim, fire) */
                u8 frameLen = ENEMY_SHOOT_RATE / 2;
                if (frameLen < 2) frameLen = 2;

                if (e->stateTimer >= frameLen) {
                    e->stateTimer = 0;
                    e->animFrame++;

                    if (e->animFrame == 1) {
                        /* Fire on second frame */
                        enemyFireAtPlayer(e, playerX, playerY, playerA);
                    }

                    if (e->animFrame >= ATTACK_ANIM_FRAMES) {
                        e->state = ES_WALK;
                        e->animTimer = 0;
                        e->animFrame = 0;
                        e->movecount = 4 + (enemyRandom() & 7);
                    }
                }
            }
            break;
        }

        case ES_PAIN:
            e->angle = fixedAtan2(dy, dx);
            e->stateTimer++;
            if (e->stateTimer >= 5) {
                e->state = ES_WALK;
                e->stateTimer = 0;
                e->animFrame = 0;
            }
            break;

        case ES_DEAD: {
            u8 maxDeathFrames = DEATH_ANIM_FRAMES;
            if (e->enemyType == ETYPE_IMP) maxDeathFrames = IMP_DEATH_ANIM_FRAMES;
            else if (e->enemyType == ETYPE_DEMON) maxDeathFrames = DEMON_DEATH_ANIM_FRAMES;
            else if (e->enemyType == ETYPE_COMMANDO) maxDeathFrames = COMMANDO_DEATH_ANIM_FRAMES;
            e->animTimer++;
            if (e->animTimer >= 4) {
                e->animTimer = 0;
                if (e->animFrame < maxDeathFrames - 1) {
                    e->animFrame++;
                }
            }
            break;
        }
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

    /* Dispatch to type-specific frame tables */
    if (e->enemyType == ETYPE_IMP)
        return getImpSpriteFrame(enemyIdx, playerX, playerY, playerA);
    if (e->enemyType == ETYPE_DEMON)
        return getDemonSpriteFrame(enemyIdx, playerX, playerY, playerA);
    if (e->enemyType == ETYPE_COMMANDO)
        return getCommandoSpriteFrame(enemyIdx, playerX, playerY, playerA);

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

u8 getImpSpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];
    u8 dir = getEnemyDirection(enemyIdx, playerX, playerY, playerA);

    switch (e->state) {
    case ES_IDLE:
        return IMP_WALK_FRAMES[dir][0];

    case ES_WALK:
        return IMP_WALK_FRAMES[dir][e->animFrame % IMP_WALK_ANIM_FRAMES];

    case ES_ATTACK: {
        u8 pose = e->animFrame;
        if (pose >= IMP_ATTACK_ANIM_FRAMES) pose = 0;
        return IMP_ATTACK_FRAMES[dir][pose];
    }

    case ES_PAIN:
        return IMP_PAIN_FRAMES[dir];

    case ES_DEAD:
        if (e->animFrame < IMP_DEATH_ANIM_FRAMES) {
            return IMP_DEATH_FRAMES[e->animFrame];
        }
        return IMP_DEATH_FRAMES[IMP_DEATH_ANIM_FRAMES - 1];

    default:
        return 0;
    }
}

u8 getDemonSpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];
    u8 dir = getEnemyDirection(enemyIdx, playerX, playerY, playerA);

    switch (e->state) {
    case ES_IDLE:
        return DEMON_WALK_FRAMES[dir][0];

    case ES_WALK:
        return DEMON_WALK_FRAMES[dir][e->animFrame % DEMON_WALK_ANIM_FRAMES];

    case ES_ATTACK: {
        u8 pose = e->animFrame;
        if (pose >= DEMON_ATTACK_ANIM_FRAMES) pose = 0;
        return DEMON_ATTACK_FRAMES[dir][pose];
    }

    case ES_PAIN:
        return DEMON_PAIN_FRAMES[dir];

    case ES_DEAD:
        if (e->animFrame < DEMON_DEATH_ANIM_FRAMES) {
            return DEMON_DEATH_FRAMES[e->animFrame];
        }
        return DEMON_DEATH_FRAMES[DEMON_DEATH_ANIM_FRAMES - 1];

    default:
        return 0;
    }
}

u8 getCommandoSpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA) {
    EnemyState *e = &g_enemies[enemyIdx];
    u8 dir = getEnemyDirection(enemyIdx, playerX, playerY, playerA);

    switch (e->state) {
    case ES_IDLE:
        return COMMANDO_WALK_FRAMES[dir][0];

    case ES_WALK:
        return COMMANDO_WALK_FRAMES[dir][e->animFrame % COMMANDO_WALK_ANIM_FRAMES];

    case ES_ATTACK: {
        u8 pose = e->animFrame;
        if (pose >= COMMANDO_ATTACK_ANIM_FRAMES) pose = 0;
        return COMMANDO_ATTACK_FRAMES[dir][pose];
    }

    case ES_PAIN:
        return COMMANDO_PAIN_FRAMES[dir];

    case ES_DEAD:
        if (e->animFrame < COMMANDO_DEATH_ANIM_FRAMES) {
            return COMMANDO_DEATH_FRAMES[e->animFrame];
        }
        return COMMANDO_DEATH_FRAMES[COMMANDO_DEATH_ANIM_FRAMES - 1];

    default:
        return 0;
    }
}

u8 getSpriteDirection(u16 sprX, u16 sprY, s16 sprAngle, u16 playerX, u16 playerY) {
    s16 dx = (s16)sprX - (s16)playerX;
    s16 dy = (s16)sprY - (s16)playerY;
    s16 angleToSprite = fixedAtan2(dy, dx);
    s16 relAngle = (angleToSprite - sprAngle + 576) & 1023;
    return (u8)((relAngle >> 7) & 7);
}
