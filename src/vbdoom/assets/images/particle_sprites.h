#ifndef __PARTICLE_SPRITES_H__
#define __PARTICLE_SPRITES_H__

#define PARTICLE_CHAR_START   764
#define PARTICLE_TILE_COUNT   178
#define PARTICLE_TILE_BYTES   2848

/* Per-frame puff sizes (pixels) and tile counts */
#define PUFF_FRAME0_SIZE    8
#define PUFF_FRAME0_TILES_W 1
#define PUFF_FRAME0_TILES   1
#define PUFF_FRAME1_SIZE    8
#define PUFF_FRAME1_TILES_W 1
#define PUFF_FRAME1_TILES   1
#define PUFF_FRAME2_SIZE    16
#define PUFF_FRAME2_TILES_W 2
#define PUFF_FRAME2_TILES   4
#define PUFF_FRAME3_SIZE    16
#define PUFF_FRAME3_TILES_W 2
#define PUFF_FRAME3_TILES   4

/* Puff map offsets (index into puffMap[]) */
#define PUFF_OFFSET0         0
#define PUFF_OFFSET1         1
#define PUFF_OFFSET2         2
#define PUFF_OFFSET3         6
#define PUFF_FRAME_COUNT      4

/* Shotgun group: 32x32 px = 4x4 tiles */
#define GROUP_TILES_W         4
#define GROUP_TILES_H         4
#define GROUP_FRAME_TILES     16
#define GROUP_VARIANT_COUNT   4
#define GROUP_FRAME_COUNT     4

extern const unsigned int particleTiles[712];
extern const unsigned short puffMap[10];
extern const unsigned short shotgunGroupMap[256];

#endif
