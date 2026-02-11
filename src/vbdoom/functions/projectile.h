#ifndef _FUNCTIONS_PROJECTILE_H
#define _FUNCTIONS_PROJECTILE_H

#include <types.h>
#include <stdbool.h>

/*
 * Projectile system (fireballs + rockets).
 * IMP enemies throw fireballs, player fires rockets.
 * Uses the particle rendering world for display.
 */

#define MAX_PROJECTILES  3  /* 2 fireballs + 1 rocket */

/* Projectile states */
#define PROJ_DEAD       0
#define PROJ_FLYING     1
#define PROJ_EXPLODING  2

/* Projectile types */
#define PROJ_TYPE_FIREBALL  0
#define PROJ_TYPE_ROCKET    1

/* Fireball constants */
#define FIREBALL_SPEED    40
#define FIREBALL_DAMAGE_MIN  3
#define FIREBALL_DAMAGE_MAX  24
#define FIREBALL_EXPLODE_NFRAMES  5
#define FIREBALL_EXPLODE_RATE    4  /* frames per explosion anim step */

/* Rocket constants */
#define ROCKET_SPEED          80     /* fast rocket (~3.3x fireball) */
#define ROCKET_DIRECT_DAMAGE  80     /* simplified: (P_Random()%8+1)*20 average */
#define ROCKET_SPLASH_RADIUS  384    /* fixed-point units (~1.5 tiles) */

typedef struct {
    u16  x, y;          /* 8.8 fixed-point position */
    s16  dx, dy;         /* velocity per frame (fixed-point) */
    u8   animFrame;      /* 0-1 flight, 0-4 exploding */
    u8   animTimer;      /* counts frames for animation */
    u8   state;          /* PROJ_DEAD / PROJ_FLYING / PROJ_EXPLODING */
    u8   sourceEnemy;    /* which enemy fired it (255 = player) */
    u8   type;           /* PROJ_TYPE_FIREBALL or PROJ_TYPE_ROCKET */
    s16  angle;          /* travel angle (for directional rendering) */
} Projectile;

extern Projectile g_projectiles[MAX_PROJECTILES];

/* Spawn a fireball from (sx,sy) aimed at (tx,ty). sourceIdx = firing enemy. */
void spawnFireball(u16 sx, u16 sy, u16 tx, u16 ty, u8 sourceIdx);

/* Spawn a rocket from player position in given angle direction */
void spawnRocket(u16 px, u16 py, s16 angle);

/* Update all projectiles: movement, collision, animation. Call once per frame. */
void updateProjectiles(u16 playerX, u16 playerY, s16 playerA);

/* Initialize projectile system */
void initProjectiles(void);

#endif
