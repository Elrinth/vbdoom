#ifndef _RAYCASTER_H_
#define _RAYCASTER_H_

#include <stdint.h>
#include <types.h>

// virtual boy has 384 x 224 resolution
#define TABLES_384
#define SCREEN_WIDTH (u16)384
#define SCREEN_HEIGHT (u16)208
#define SCREEN_H_DIV_2 (u16)(SCREEN_HEIGHT>>1)

#define SCREEN_SCALE 1
#define FOV (double)(M_PI / 2)
#define INV_FACTOR (float)(SCREEN_WIDTH * 95.0f / 320.0f)
#define LOOKUP_TBL
#define LOOKUP8(tbl, offset) tbl[offset]
#define LOOKUP16(tbl, offset) tbl[offset]

#define HFOV = (u16)(0.7 * SCREEN_HEIGHT) // affects horizontal field of vision
#define VFOV = (u16)(0.8 * SCREEN_HEIGHT) // affects vertical field of vision
//#define HFOV = (u16)(0.6 * SCREEN_HEIGHT) // affects horizontal field of vision
//#define VFOV = (u16)(0.26 * SCREEN_HEIGHT) // affects vertical field of vision

//hfov = 0.7 * H; // * aspectW; // * H;
//vfov = 0.8 * H; // * aspectW; // * H;

#define MAP_X     (u8)64
#define MAP_Y     (u8)64
#define MAP_XS    (u8)6   /* log2(MAP_X) for shift */
#define MAP_CELLS ((u16)(MAP_X) * (u16)(MAP_Y))
#define INV_FACTOR_INT ((u16)(SCREEN_WIDTH * 75))
#define MIN_DIST (int)((150 * ((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)))
#define HORIZON_HEIGHT (SCREEN_HEIGHT / 2)
#define INVERT(x) (u8)((x ^ 255) + 1)
#define ABS(x) (x < 0 ? -x : x)

/* Raycaster column step: 8 = full resolution (48 columns), 16 = half (24 columns). */
#define RAYCAST_STEP 8
#define RAYCAST_COLS (SCREEN_WIDTH / RAYCAST_STEP)
#define RAYCAST_CENTER_COL (192 / RAYCAST_STEP)  /* screen center for USE/door activation */


#endif