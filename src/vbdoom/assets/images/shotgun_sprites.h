#ifndef __SHOTGUN_SPRITES_H__
#define __SHOTGUN_SPRITES_H__

#define SHOTGUN_CHAR_START   120
#define SHOTGUN_TILE_COUNT   342
#define SHOTGUN_TILE_BYTES   5472

extern const unsigned int shotgunTiles[1368];

/* Red layer frames */
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

/* Black layer frames */
#define SHOTGUN_BLK_F0_XTILES  16  /* 8 cols x 2 bytes */
#define SHOTGUN_BLK_F0_ROWS    6
extern const unsigned short shotgunBlkFrame0Map[48];

#define SHOTGUN_BLK_F1_XTILES  16  /* 8 cols x 2 bytes */
#define SHOTGUN_BLK_F1_ROWS    8
extern const unsigned short shotgunBlkFrame1Map[64];

#define SHOTGUN_BLK_F2_XTILES  16  /* 8 cols x 2 bytes */
#define SHOTGUN_BLK_F2_ROWS    8
extern const unsigned short shotgunBlkFrame2Map[64];

#define SHOTGUN_BLK_F3_XTILES  14  /* 7 cols x 2 bytes */
#define SHOTGUN_BLK_F3_ROWS    8
extern const unsigned short shotgunBlkFrame3Map[56];

#define SHOTGUN_BLK_F4_XTILES  10  /* 5 cols x 2 bytes */
#define SHOTGUN_BLK_F4_ROWS    8
extern const unsigned short shotgunBlkFrame4Map[40];

#define SHOTGUN_BLK_F5_XTILES  14  /* 7 cols x 2 bytes */
#define SHOTGUN_BLK_F5_ROWS    8
extern const unsigned short shotgunBlkFrame5Map[56];

#endif
