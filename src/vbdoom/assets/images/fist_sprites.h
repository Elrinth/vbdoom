#ifndef __FIST_SPRITES_H__
#define __FIST_SPRITES_H__

#define FIST_TILE_COUNT   213
#define FIST_TILE_BYTES   3408

extern const unsigned int fistTiles[852];

#define FIST_FRAME_COUNT  4
#define FIST_F0_XTILES  30
#define FIST_F0_ROWS    6
extern const unsigned short fistFrame0Map[90];
#define FIST_F1_XTILES  22
#define FIST_F1_ROWS    6
extern const unsigned short fistFrame1Map[66];
#define FIST_F2_XTILES  28
#define FIST_F2_ROWS    7
extern const unsigned short fistFrame2Map[98];
#define FIST_F3_XTILES  34
#define FIST_F3_ROWS    8
extern const unsigned short fistFrame3Map[136];

#endif
