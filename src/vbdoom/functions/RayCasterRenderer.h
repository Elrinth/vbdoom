#ifndef _FUNCTIONS_RAYCASTERRENDERER_H
#define _FUNCTIONS_RAYCASTERRENDERER_H

#include <stdint.h>
typedef int64_t s64;
u32 GetARGB(u8 brightness);
void UpdateCenterRay(u16 playerX, u16 playerY, s16 playerA);
void TraceFrame(u16 *playerX, u16 *playerY, s16 *playerA);
void clearTiles(u8 bgmap);

void affine_fast_scale_fixed2(u8 world, f16 scale);

#define clamp(a, mi,ma)      min(max(a,mi),ma)
#define min(a,b)             (((a) < (b)) ? (a) : (b)) // min: Choose smaller of two scalars.
#define max(a,b)             (((a) > (b)) ? (a) : (b)) // max: Choose greater of two scalars.

#endif