#ifndef _FUNCTIONS_ENEMY_H
#define _FUNCTIONS_ENEMY_H

#include <types.h>
#include <stdbool.h>

/*
 * Enemy system: animation, AI, and multi-enemy support.
 * Max 3 enemies on screen (1 world per enemy).
 */

#define MAX_ENEMIES     21  /* E1M4 uses 21; other levels use up to 13 */
#define MAX_VISIBLE_ENEMIES 5  /* only 5 can render at once (VRAM/BGMap slots) */
#define ENEMY_SPEED     8     /* fixed-point movement speed per frame */
#define ENEMY_ATTACK_DIST  2500  /* squared-distance/256 for ranged attack checks */
#define ENEMY_MIN_DIST     150   /* minimum distance to player (don't walk closer) */
#define ENEMY_WALK_RATE 6     /* frames between walk animation advances */
#define ENEMY_SHOOT_RATE 45   /* frames between attack animation poses (~2.25s at 20fps) */
#define ENEMY_SIGHT_DIST 3000 /* squared-distance/256 at which enemies notice player (~7 tiles) */

/* Doom hitscan range: 2048 map units. In our 8.8 fixed-point (256=1 tile):
 * 2048 units ≈ 8 tiles in Doom scale; our 32 tiles ≈ 8192.
 * dist = (dx^2+dy^2)>>8; for 32 tiles: 8192^2>>8 = 262144 */
#define HITSCAN_RANGE_DIST  262144

/* Fist melee range: squared-dist/256.  400 ≈ sqrt(400*256) ≈ 320 units ≈ 1.25 tiles */
#define FIST_MELEE_RANGE    400

/* Collision radii */
#define ENEMY_RADIUS    80
#define PLAYER_RADIUS   64

/* Enemy types (Doom) */
#define ETYPE_ZOMBIEMAN  0
#define ETYPE_SERGEANT   1
#define ETYPE_IMP        2
#define ETYPE_DEMON      3

/* Per-type stats */
#define ZOMBIE_HEALTH       20
#define ZOMBIE_PAINCHANCE   200   /* out of 256 */
#define SGT_HEALTH          30
#define SGT_PAINCHANCE      170   /* out of 256 */
#define IMP_HEALTH          60
#define IMP_PAINCHANCE      200   /* out of 256 */
#define IMP_MELEE_DIST      150   /* squared-dist/256 for claw attack range */
#define DEMON_HEALTH        150
#define DEMON_PAINCHANCE    180   /* out of 256 */
#define DEMON_MELEE_DIST    200   /* squared-dist/256 for bite range */

/* Enemy states */
#define ES_IDLE   0  /* standing still, unaware of player */
#define ES_WALK   1  /* alerted, chasing player */
#define ES_ATTACK 2  /* shooting at player */
#define ES_PAIN   3  /* flinching from damage */
#define ES_DEAD   4  /* dead */

/* Movement directions (from Doom p_enemy.c) */
#define DI_EAST       0
#define DI_NORTHEAST  1
#define DI_NORTH      2
#define DI_NORTHWEST  3
#define DI_WEST       4
#define DI_SOUTHWEST  5
#define DI_SOUTH      6
#define DI_SOUTHEAST  7
#define DI_NODIR      8

typedef struct {
    u16  x;              /* fixed-point position (8.8: upper=tile, lower=sub) */
    u16  y;
    s16  angle;          /* facing angle (0-1023, toward player) */
    u8   state;          /* ES_WALK / ES_ATTACK / ES_PAIN / ES_DEAD */
    u8   animFrame;      /* current frame within the animation state */
    u8   animTimer;      /* counts up, triggers frame advance */
    u8   stateTimer;     /* general-purpose timer for state transitions */
    u8   lastRenderedFrame; /* last frame index sent to char memory (skip if same) */
    u8   health;
    u8   movedir;        /* current movement direction (DI_EAST..DI_NODIR) */
    u8   movecount;      /* frames left before picking new direction */
    u8   enemyType;      /* ETYPE_ZOMBIEMAN or ETYPE_SERGEANT */
    bool active;         /* false = don't update or render */
} EnemyState;

extern EnemyState g_enemies[MAX_ENEMIES];

/* Indices of the MAX_VISIBLE_ENEMIES closest active enemies (set each frame) */
extern u8 g_visibleEnemies[MAX_VISIBLE_ENEMIES];
extern u8 g_numVisibleEnemies;

/*
 * Frame lookup tables.
 * 49-frame zombie sprite sheet. Exact layout per the extracted sprites:
 *
 *  Forward:       walk 0,1,2,3     shoot 4,5
 *  Forward-left:  walk 6,7,8,9     shoot 10,11
 *  Left:          walk 12,13,14,15 shoot 16,17
 *  Back-left:     walk 18,19,20,23 shoot 21,22  (note: 23 is walk frame 4!)
 *  Back:          walk 24,25,26,27 shoot 28,29
 *
 *  Pain (1 per direction): 30=fwd, 31=fwd-left, 32=left, 33=back-left, 34=back
 *  Death: 35-39
 *  Gib:   40-48
 *
 *  Directions 5-7 (back-right, right, front-right) mirror 3-1.
 */
#define WALK_ANIM_FRAMES  4
#define DEATH_ANIM_FRAMES 5
#define ATTACK_ANIM_FRAMES 2

/* Walk frames: [direction][pose] -> sprite index */
static const u8 WALK_FRAMES[8][WALK_ANIM_FRAMES] = {
    { 0,  1,  2,  3},   /* dir 0: forward */
    { 6,  7,  8,  9},   /* dir 1: forward-left */
    {12, 13, 14, 15},   /* dir 2: left */
    {18, 19, 20, 23},   /* dir 3: back-left (note: 23 not 21!) */
    {24, 25, 26, 27},   /* dir 4: back */
    {18, 19, 20, 23},   /* dir 5: back-right  (mirror of 3) */
    {12, 13, 14, 15},   /* dir 6: right        (mirror of 2) */
    { 6,  7,  8,  9},   /* dir 7: front-right  (mirror of 1) */
};

/* Attack frames: [direction][pose] -> sprite index */
static const u8 ATTACK_FRAMES[8][ATTACK_ANIM_FRAMES] = {
    { 4,  5},   /* dir 0: forward */
    {10, 11},   /* dir 1: forward-left */
    {16, 17},   /* dir 2: left */
    {21, 22},   /* dir 3: back-left */
    {28, 29},   /* dir 4: back */
    {21, 22},   /* dir 5: back-right  (mirror of 3) */
    {16, 17},   /* dir 6: right        (mirror of 2) */
    {10, 11},   /* dir 7: front-right  (mirror of 1) */
};

/* Shoot frames (legacy): first attack pose per direction */
static const u8 SHOOT_FRAMES[8] = {4, 10, 16, 21, 28, 21, 16, 10};

/* Pain frames: [direction] -> sprite index (one per direction) */
static const u8 PAIN_FRAMES[8] = {30, 31, 32, 33, 34, 33, 32, 31};

/* Death frames: sequential, no direction */
static const u8 DEATH_FRAMES[DEATH_ANIM_FRAMES] = {35, 36, 37, 38, 39};

/*
 * IMP frame lookup tables.
 * 56-frame sprite sheet. Layout (7 frames per direction):
 *
 *   Dir0 (front):    walk 2,4,0,3    attack 1,5,6       (frames 0-6)
 *   Dir1 (fwd-left): walk 10,8,11,7  attack 9,12,13     (frames 7-13)
 *   Dir2 (left):     walk 17,15,20,14 attack 18,19,16   (frames 14-20)
 *   Dir3 (back-left):walk 22,23,21,27 attack 24,25,26   (frames 21-27)
 *   Dir4 (back):     walk 28,32,29,33 attack 31,34,30   (frames 28-34)
 *
 *   Pain (35-39): left, back, front-left, back-left, front
 *   Death (40-44): 5 frames
 *   Gib (45-47, 51-55): 8 frames
 */
#define IMP_WALK_ANIM_FRAMES   4
#define IMP_ATTACK_ANIM_FRAMES 3
#define IMP_DEATH_ANIM_FRAMES  5

static const u8 IMP_WALK_FRAMES[8][IMP_WALK_ANIM_FRAMES] = {
    { 2,  4,  0,  3},   /* dir 0: front */
    {10,  8, 11,  7},   /* dir 1: front-left */
    {17, 15, 20, 14},   /* dir 2: left */
    {22, 23, 21, 27},   /* dir 3: back-left */
    {28, 32, 29, 33},   /* dir 4: back */
    {22, 23, 21, 27},   /* dir 5: back-right  (mirror of 3) */
    {17, 15, 20, 14},   /* dir 6: right        (mirror of 2) */
    {10,  8, 11,  7},   /* dir 7: front-right  (mirror of 1) */
};

static const u8 IMP_ATTACK_FRAMES[8][IMP_ATTACK_ANIM_FRAMES] = {
    { 1,  5,  6},   /* dir 0: front */
    { 9, 12, 13},   /* dir 1: front-left */
    {18, 19, 16},   /* dir 2: left */
    {24, 25, 26},   /* dir 3: back-left */
    {31, 34, 30},   /* dir 4: back */
    {24, 25, 26},   /* dir 5: back-right  (mirror of 3) */
    {18, 19, 16},   /* dir 6: right        (mirror of 2) */
    { 9, 12, 13},   /* dir 7: front-right  (mirror of 1) */
};

/* Pain: 35=left, 36=back, 37=front-left, 38=back-left, 39=front */
static const u8 IMP_PAIN_FRAMES[8] = {39, 37, 35, 38, 36, 38, 35, 37};

static const u8 IMP_DEATH_FRAMES[IMP_DEATH_ANIM_FRAMES] = {40, 41, 42, 43, 44};

/*
 * DEMON (Pinky) frame lookup tables.
 * 46 frames (was 70; right-facing dirs 5-7 mirror left-facing 3-1).
 *
 *   Dir0 (front):       walk 0,1,2,3     attack 4,5,6       (frames 0-6)
 *   Dir1 (front-left):  walk 7,8,9,10    attack 11,12,13    (frames 7-13)
 *   Dir2 (left):        walk 14,15,16,17 attack 18,19,20    (frames 14-20)
 *   Dir3 (back-left):   walk 21,22,23,24 attack 25,26,27    (frames 21-27)
 *   Dir4 (back):        walk 28,29,30,31 attack 32,33,34    (frames 28-34)
 *   Dir5-7: reuse Dir3-1 (H-flipped by renderer)
 *
 *   Pain: 35=front, 36=front-left, 37=left, 38=back-left, 39=back
 *   Death (40-45): 6 frames
 */
#define DEMON_WALK_ANIM_FRAMES   4
#define DEMON_ATTACK_ANIM_FRAMES 3
#define DEMON_DEATH_ANIM_FRAMES  6

static const u8 DEMON_WALK_FRAMES[8][DEMON_WALK_ANIM_FRAMES] = {
    { 2,  0,  3,  1},   /* dir 0: front */
    {10,  7, 13,  9},   /* dir 1: front-left */
    {16, 14, 17, 15},   /* dir 2: left */
    {22, 23, 24, 21},   /* dir 3: back-left */
    {28, 31, 29, 32},   /* dir 4: back */
    {22, 23, 24, 21},   /* dir 5: back-right  (mirror of 3) */
    {16, 14, 17, 15},   /* dir 6: right        (mirror of 2) */
    {10,  7, 13,  9},   /* dir 7: front-right  (mirror of 1) */
};

static const u8 DEMON_ATTACK_FRAMES[8][DEMON_ATTACK_ANIM_FRAMES] = {
    { 4,  5,  6},   /* dir 0: front */
    {11, 12,  8},   /* dir 1: front-left */
    {19, 20, 18},   /* dir 2: left */
    {27, 26, 25},   /* dir 3: back-left */
    {34, 33, 30},   /* dir 4: back */
    {27, 26, 25},   /* dir 5: back-right  (mirror of 3) */
    {19, 20, 18},   /* dir 6: right        (mirror of 2) */
    {11, 12,  8},   /* dir 7: front-right  (mirror of 1) */
};

/* Pain: one per direction (renumbered: old 56->35, 57->36, 59->37, 61->38, 62->39) */
static const u8 DEMON_PAIN_FRAMES[8] = {35, 36, 37, 38, 39, 38, 37, 36};

static const u8 DEMON_DEATH_FRAMES[DEMON_DEATH_ANIM_FRAMES] = {40, 41, 42, 43, 44, 45};

/* ---- API ---- */

/* Initialize all enemies (positions, state). Call once at level start. */
void initEnemies(void);
void initEnemiesE1M2(void);
void initEnemiesE1M3(void);
void initEnemiesE1M4(void);

/* Update AI and animation for all enemies. Call once per frame. */
void updateEnemies(u16 playerX, u16 playerY, s16 playerA);

/* Get the sprite frame index for an enemy, accounting for direction relative to player view. */
u8 getEnemySpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA);

/* Compute direction index (0-7) from enemy toward player, relative to player's view angle. */
u8 getEnemyDirection(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA);

/* Doom-style AABB collision: check if position (x,y) with given radius
 * overlaps any active enemy. skipIdx=255 means don't skip any (use for player).
 * Returns true if blocked. */
bool collidesWithAnyEnemy(u16 x, u16 y, u16 myRadius, u8 skipIdx);

/* Alert all enemies (e.g. player fired a weapon). In Doom, gunshots
 * propagate through connected sectors and wake up nearby enemies. */
void alertAllEnemies(void);

/* Hitscan: player shoots, checks if any enemy in the aiming cone is hit.
 * weaponType: 1=fist, 2=pistol, 3=shotgun.
 * Shotgun fires 7 pellets with Doom-style angular spread.
 * Returns index of a hit enemy (0-MAX_ENEMIES-1) or 255 if all miss.
 * Based on Doom's P_AimLineAttack + P_LineAttack (simplified). */
u8 playerShoot(u16 playerX, u16 playerY, s16 playerA, u8 weaponType);

/* --- Damage feedback (set by updateEnemies, read+cleared by game loop) --- */
extern u8 g_lastEnemyDamage;     /* damage dealt to player this frame (0=none) */
extern s8 g_lastEnemyDamageDir;  /* -1=left, 0=front, 1=right */

/* --- Kill tracking (for level stats screen) --- */
extern u8 g_enemiesKilled;
extern u8 g_totalEnemies;

/* --- Sound event flags (set by enemy.c, read+cleared by sndplay.c) --- */
extern u8 g_enemySndAggro;       /* >0: enemy just became aggro, value = distance/16 */
extern u8 g_enemySndShoot;       /* >0: enemy just fired, value = distance/16 */
extern u8 g_enemySndShootType;   /* 0=pistol (zombieman), 1=shotgun (sergeant) */
extern u8 g_enemySndDeath;       /* >0: enemy just died, value = distance/16 */

/* Returns true if the enemy at bestIdx just died (state == ES_DEAD) after a shot */
#define ENEMY_JUST_KILLED(idx) ((idx) < MAX_ENEMIES && g_enemies[idx].state == ES_DEAD)

/* Get IMP-specific sprite frame index (56-frame sheet) */
u8 getImpSpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA);

/* Get Demon-specific sprite frame index (46-frame sheet) */
u8 getDemonSpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA);

/* Apply damage to an enemy, handle kill/pain/drops/sounds. Returns true if killed. */
bool applyDamageToEnemy(EnemyState *e, u8 damage, u16 playerX, u16 playerY);

/* Approximate distance (Manhattan-ish) */
u16 approxDist(s16 dx, s16 dy);

#endif
