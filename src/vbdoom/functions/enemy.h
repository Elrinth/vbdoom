#ifndef _FUNCTIONS_ENEMY_H
#define _FUNCTIONS_ENEMY_H

#include <types.h>
#include <stdbool.h>

/*
 * Enemy system: animation, AI, and multi-enemy support.
 * Max 2 enemies on screen (1 world per enemy, Option C single-layer).
 */

#define MAX_ENEMIES     3
#define ENEMY_SPEED     8     /* fixed-point movement speed per frame */
#define ENEMY_ATTACK_DIST  400  /* distance (fixed-point) to start shooting */
#define ENEMY_MIN_DIST     150  /* minimum distance to player (don't walk closer) */
#define ENEMY_WALK_RATE 6     /* frames between walk animation advances */
#define ENEMY_SHOOT_RATE 20   /* frames between shots */

/* Enemy states */
#define ES_WALK   0
#define ES_ATTACK 1
#define ES_PAIN   2
#define ES_DEAD   3

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
    bool active;         /* false = don't update or render */
} EnemyState;

extern EnemyState g_enemies[MAX_ENEMIES];

/*
 * Frame lookup tables.
 * Based on the 49-frame zombie sprite sheet (extracted left-to-right, top-to-bottom).
 * 8 directions: 0=facing player, 1=front-right, 2=right, ... 7=front-left (clockwise).
 *
 * Walk: 2 alternating poses x 8 directions (rows 0 and 3 of the sprite sheet)
 * Shoot: 1 pose x 8 directions (row 2)
 * Death: 6 sequential frames (no direction, rows 5-6)
 *
 * These can be fine-tuned after seeing them in-game.
 */
#define WALK_ANIM_FRAMES  2
#define DEATH_ANIM_FRAMES 6

/* Walk frames: [direction][pose] -> sprite index */
static const u8 WALK_FRAMES[8][WALK_ANIM_FRAMES] = {
    { 0, 22},   /* dir 0: front (facing player) */
    { 1, 23},   /* dir 1: front-right */
    { 2, 24},   /* dir 2: right */
    { 3, 25},   /* dir 3: back-right */
    { 4, 26},   /* dir 4: back */
    { 5, 27},   /* dir 5: back-left */
    { 6, 28},   /* dir 6: left */
    { 7, 29},   /* dir 7: front-left */
};

/* Shoot frames: [direction] -> sprite index */
static const u8 SHOOT_FRAMES[8] = {15, 16, 17, 18, 19, 20, 21, 15};

/* Death frames: sequential, no direction */
static const u8 DEATH_FRAMES[DEATH_ANIM_FRAMES] = {37, 38, 39, 40, 41, 42};

/* ---- API ---- */

/* Initialize all enemies (positions, state). Call once at game start. */
void initEnemies(void);

/* Update AI and animation for all enemies. Call once per frame. */
void updateEnemies(u16 playerX, u16 playerY, s16 playerA);

/* Get the sprite frame index for an enemy, accounting for direction relative to player view. */
u8 getEnemySpriteFrame(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA);

/* Compute direction index (0-7) from enemy toward player, relative to player's view angle. */
u8 getEnemyDirection(u8 enemyIdx, u16 playerX, u16 playerY, s16 playerA);

#endif
