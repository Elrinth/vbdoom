#ifndef __SHOTGUN_SPRITES_H__
#define __SHOTGUN_SPRITES_H__

#define SHOTGUN_TILE_COUNT   215
#define SHOTGUN_TILE_BYTES   3440

extern const unsigned int shotgunTiles[860];

#define SHOTGUN_F0_XTILES  16  /* 8 cols x 2 bytes */
#define SHOTGUN_F0_ROWS    6
extern const unsigned short shotgunFrame0Map[48];

#define SHOTGUN_F1_XTILES  16  /* 8 cols x 2 bytes */
#define SHOTGUN_F1_ROWS    8
extern const unsigned short shotgunFrame1Map[64];

#define SHOTGUN_F2_XTILES  16  /* 8 cols x 2 bytes */
#define SHOTGUN_F2_ROWS    8
extern const unsigned short shotgunFrame2Map[64];

#define SHOTGUN_F3_XTILES  14  /* 7 cols x 2 bytes */
#define SHOTGUN_F3_ROWS    8
extern const unsigned short shotgunFrame3Map[56];

#define SHOTGUN_F4_XTILES  10  /* 5 cols x 2 bytes */
#define SHOTGUN_F4_ROWS    8
extern const unsigned short shotgunFrame4Map[40];

#define SHOTGUN_F5_XTILES  14  /* 7 cols x 2 bytes */
#define SHOTGUN_F5_ROWS    8
extern const unsigned short shotgunFrame5Map[56];

#endif
