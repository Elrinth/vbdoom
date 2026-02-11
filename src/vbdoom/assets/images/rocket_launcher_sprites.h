#ifndef __ROCKET_LAUNCHER_SPRITES_H__
#define __ROCKET_LAUNCHER_SPRITES_H__

#define ROCKET_LAUNCHER_CHAR_START   120
#define ROCKET_LAUNCHER_TILE_COUNT   241
#define ROCKET_LAUNCHER_TILE_BYTES   3856

extern const unsigned int rocketLauncherTiles[964];

/* Red layer frames */
#define ROCKET_F0_XTILES  10  /* 5 cols x 2 bytes */
#define ROCKET_F0_ROWS    4
extern const unsigned short rocketLauncherFrame0Map[20];

#define ROCKET_F1_XTILES  10  /* 5 cols x 2 bytes */
#define ROCKET_F1_ROWS    4
extern const unsigned short rocketLauncherFrame1Map[20];

#define ROCKET_F2_XTILES  12  /* 6 cols x 2 bytes */
#define ROCKET_F2_ROWS    5
extern const unsigned short rocketLauncherFrame2Map[30];

#define ROCKET_F3_XTILES  12  /* 6 cols x 2 bytes */
#define ROCKET_F3_ROWS    5
extern const unsigned short rocketLauncherFrame3Map[30];

#define ROCKET_F4_XTILES  16  /* 8 cols x 2 bytes */
#define ROCKET_F4_ROWS    6
extern const unsigned short rocketLauncherFrame4Map[48];

#define ROCKET_F5_XTILES  10  /* 5 cols x 2 bytes */
#define ROCKET_F5_ROWS    3
extern const unsigned short rocketLauncherFrame5Map[15];

/* Black layer frames */
#define ROCKET_BLK_F0_XTILES  10  /* 5 cols x 2 bytes */
#define ROCKET_BLK_F0_ROWS    4
extern const unsigned short rocketLauncherBlkFrame0Map[20];

#define ROCKET_BLK_F1_XTILES  10  /* 5 cols x 2 bytes */
#define ROCKET_BLK_F1_ROWS    4
extern const unsigned short rocketLauncherBlkFrame1Map[20];

#define ROCKET_BLK_F2_XTILES  12  /* 6 cols x 2 bytes */
#define ROCKET_BLK_F2_ROWS    5
extern const unsigned short rocketLauncherBlkFrame2Map[30];

#define ROCKET_BLK_F3_XTILES  12  /* 6 cols x 2 bytes */
#define ROCKET_BLK_F3_ROWS    5
extern const unsigned short rocketLauncherBlkFrame3Map[30];

#define ROCKET_BLK_F4_XTILES  16  /* 8 cols x 2 bytes */
#define ROCKET_BLK_F4_ROWS    6
extern const unsigned short rocketLauncherBlkFrame4Map[48];

#define ROCKET_BLK_F5_XTILES  10  /* 5 cols x 2 bytes */
#define ROCKET_BLK_F5_ROWS    3
extern const unsigned short rocketLauncherBlkFrame5Map[15];

#endif
