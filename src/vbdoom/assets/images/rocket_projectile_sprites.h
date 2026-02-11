#ifndef __ROCKET_PROJECTILE_SPRITES_H__
#define __ROCKET_PROJECTILE_SPRITES_H__

#define ROCKET_PROJ_TILE_W     2
#define ROCKET_PROJ_TILE_H     2
#define ROCKET_PROJ_TILES      4
#define ROCKET_PROJ_FRAME_BYTES 64
#define ROCKET_PROJ_FRAME_COUNT 5

extern const unsigned int rocketProjFrame0Tiles[16];
extern const unsigned int rocketProjFrame1Tiles[16];
extern const unsigned int rocketProjFrame2Tiles[16];
extern const unsigned int rocketProjFrame3Tiles[16];
extern const unsigned int rocketProjFrame4Tiles[16];

/* Frame pointer table (5 unique views) */
static const unsigned int* const ROCKET_PROJ_FRAMES[5] = {
    rocketProjFrame0Tiles,
    rocketProjFrame1Tiles,
    rocketProjFrame2Tiles,
    rocketProjFrame3Tiles,
    rocketProjFrame4Tiles
};

/*
 * Direction mapping (8 directions, 0=front/towards player):
 *   0: front        = frame 0
 *   1: front-right  = frame 4
 *   2: right        = frame 3
 *   3: back-right   = frame 2
 *   4: back         = frame 1
 *   5: back-left    = frame 2 (H-flip)
 *   6: left         = frame 3 (H-flip)
 *   7: front-left   = frame 4 (H-flip)
 */
static const u8 ROCKET_PROJ_DIR_FRAME[8] = {0, 4, 3, 2, 1, 2, 3, 4};
static const u8 ROCKET_PROJ_DIR_HFLIP[8] = {0, 0, 0, 0, 0, 1, 1, 1};

#endif
