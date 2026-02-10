/* particle.c -- Bullet puff / shotgun group particle system */

#include <libgccvb.h>
#include <mem.h>
#include "particle.h"
#include "doomgfx.h"
#include "../assets/images/particle_sprites.h"

Particle g_particles[MAX_PARTICLES];

static u8 g_groupVariantCounter = 0;

void initParticles(void)
{
    u8 i;
    for (i = 0; i < MAX_PARTICLES; i++) {
        g_particles[i].active = 0;
    }
}

void loadParticleTiles(void)
{
    u32 addr = 0x00078000 + (u32)PARTICLE_CHAR_START * 16;
    copymem((void*)addr, (void*)particleTiles, PARTICLE_TILE_BYTES);
}

void spawnPuff(s16 x, s16 y)
{
    u8 i;
    /* Find free slot (or oldest) */
    u8 best = 0;
    u8 bestFrame = 0;
    for (i = 0; i < MAX_PARTICLES; i++) {
        if (!g_particles[i].active) { best = i; break; }
        if (g_particles[i].frame > bestFrame) {
            bestFrame = g_particles[i].frame;
            best = i;
        }
    }
    g_particles[best].active = 1;
    g_particles[best].x = x;
    g_particles[best].y = y;
    g_particles[best].frame = 0;
    g_particles[best].timer = PARTICLE_FRAME_TICKS;
    g_particles[best].isGroup = 0;
    g_particles[best].variant = 0;
}

void spawnShotgunGroup(s16 x, s16 y)
{
    u8 i, best = 0;
    u8 bestFrame = 0;
    for (i = 0; i < MAX_PARTICLES; i++) {
        if (!g_particles[i].active) { best = i; break; }
        if (g_particles[i].frame > bestFrame) {
            bestFrame = g_particles[i].frame;
            best = i;
        }
    }
    g_particles[best].active = 1;
    g_particles[best].x = x;
    g_particles[best].y = y;
    g_particles[best].frame = 0;
    g_particles[best].timer = PARTICLE_FRAME_TICKS;
    g_particles[best].isGroup = 1;
    g_particles[best].variant = g_groupVariantCounter & 3;
    g_groupVariantCounter++;
}

void updateParticles(void)
{
    u8 i;
    for (i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g_particles[i];
        if (!p->active) continue;

        if (p->timer > 0) {
            p->timer--;
        } else {
            p->frame++;
            if (p->frame >= 4) {
                p->active = 0;
                continue;
            }
            p->timer = PARTICLE_FRAME_TICKS;
        }
    }
}
