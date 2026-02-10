#ifndef __PISTOL_SPRITES_H__
#define __PISTOL_SPRITES_H__

#define PISTOL_TILE_COUNT   119
#define PISTOL_TILE_BYTES   1904

extern const unsigned int pistolTiles[476];

/* Red layer frames */
#define PISTOL_RED_F0_XTILES  16
#define PISTOL_RED_F0_ROWS    7
extern const unsigned short pistolRedFrame0Map[56];
#define PISTOL_RED_F1_XTILES  16
#define PISTOL_RED_F1_ROWS    11
extern const unsigned short pistolRedFrame1Map[88];

/* Black layer frames */
#define PISTOL_BLK_F0_XTILES  16
#define PISTOL_BLK_F0_ROWS    8
extern const unsigned short pistolBlackFrame0Map[64];
#define PISTOL_BLK_F1_XTILES  16
#define PISTOL_BLK_F1_ROWS    8
extern const unsigned short pistolBlackFrame1Map[64];

#endif
