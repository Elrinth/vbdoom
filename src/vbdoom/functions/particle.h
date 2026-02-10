#ifndef __PARTICLE_H__
#define __PARTICLE_H__

#include <types.h>

#define MAX_PARTICLES     4    /* total tracked particles */
#define MAX_VIS_PARTICLES 1    /* rendered at once (1 world slot) */
#define PARTICLE_WORLD    24   /* VB world for particle rendering */
#define PARTICLE_BGMAP    9    /* BGMap slot for particle (was LAYER_BULLET) */

#define PARTICLE_LIFETIME 12   /* total frames for full animation */
#define PARTICLE_FRAME_TICKS 3 /* frames per animation step (4 steps x 3 = 12) */

typedef struct {
    u8  active;
    s16 x, y;         /* map position (same coord system as enemies) */
    u8  frame;         /* animation frame 0-3 */
    u8  timer;         /* ticks until next frame */
    u8  isGroup;       /* 0 = single puff, 1 = shotgun group */
    u8  variant;       /* group variant index (0-3) */
} Particle;

extern Particle g_particles[MAX_PARTICLES];

void initParticles(void);
void spawnPuff(s16 x, s16 y);
void spawnShotgunGroup(s16 x, s16 y);
void updateParticles(void);
void loadParticleTiles(void);

#endif
