#ifndef _FUNCTIONS_RAYCASTERDATA_H
#define _FUNCTIONS_RAYCASTERDATA_H

#include "RayCaster.h"

/* LOOKUP_TBL left as in RayCaster.h (empty); PROGMEM not used on V810 */

/*
 * Active map buffer (MAP_X*MAP_Y bytes), written by loadLevel().
 * Modified at runtime by door open/close, so must be writable (RAM).
 *
 * Tile values:
 *   0 = empty/walkable
 *   1 = STARTAN wall (brick)
 *   2 = STONE wall (stone blocks)
 *   3 = TECH wall (tech panels)
 *   4 = DOOR (opens upward when activated)
 *   5 = SWITCH (activatable wall)
 *   6,7,8 = secret doors (look like brick/stone/tech, open like doors)
 *   9,10,11 = key doors (red/yellow/blue) when implemented
 */
u8 g_map[MAP_CELLS];

/*
 * E1M1 source map (read-only). Copied into g_map by loadLevel(1).
 *
 * Layout:
 *   South (rows 26-29): Start room - pistol start
 *   South-center (rows 20-25): Main hall with first enemies
 *   West side room (rows 20-24): Health/ammo room
 *   East side room (rows 20-24): Shells box room
 *   Center (rows 12-19): Zigzag room with IMPs and sergeants
 *   West (rows 6-11): Armor room behind door
 *   North-center (rows 4-11): Computer room
 *   North-east (rows 1-5): Exit room with exit switch
 *   East corridor (cols 28-29, rows 5-11): connects east hallway to exit room
 *   Door at row 19 center: passage from main hall to zigzag
 *   Door at row 11 west: entrance to armor room (opened by switch at (18,10))
 *   Switch at (18,10): opens armor room door
 *   Exit switch at (27,3): ends level
 *
 * Player starts at approximately tile (15, 28) = (15*256+128, 28*256+128)
 */
const u8 e1m1_map[1024] = {
/* Row  0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* Row  1 */ 1,3,3,3,3,3,3,3,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3,3,1,
/* Row  2 */ 1,3,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,3,1,
/* Row  3 */ 1,3,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,5,0,0,3,1,
/* Row  4 */ 1,3,0,0,0,0,0,0,3,1,3,3,3,3,0,0,0,0,3,3,3,3,1,3,0,0,0,0,0,0,3,1,
/* Row  5 */ 1,3,3,3,3,0,3,3,3,1,3,0,0,0,0,0,0,0,0,0,0,3,1,3,3,3,3,3,0,0,3,1,
/* Row  6 */ 1,1,1,1,1,0,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,0,0,1,1,
/* Row  7 */ 1,2,2,2,2,0,2,2,2,1,3,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,0,0,1,1,
/* Row  8 */ 1,2,0,0,0,0,0,0,2,1,3,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,0,0,1,1,
/* Row  9 */ 1,2,0,0,0,0,0,0,2,1,3,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,0,0,1,1,
/* Row 10 */ 1,2,0,0,0,0,0,0,2,1,3,3,3,3,0,0,0,0,5,3,3,3,1,1,1,1,1,1,0,0,1,1,
/* Row 11 */ 1,2,2,2,2,4,2,2,2,1,1,1,1,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,0,0,2,1,
/* Row 12 */ 1,0,0,0,0,0,1,1,1,2,2,2,0,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,2,1,
/* Row 13 */ 2,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,1,
/* Row 14 */ 2,0,0,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,2,1,
/* Row 15 */ 2,0,0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,1,
/* Row 16 */ 2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,1,
/* Row 17 */ 2,0,0,0,0,0,0,0,0,0,2,0,0,2,2,0,0,0,2,2,0,0,1,1,1,1,1,1,1,1,1,1,
/* Row 18 */ 2,0,0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,
/* Row 19 */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,4,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,
/* Row 20 */ 1,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,
/* Row 21 */ 1,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,
/* Row 22 */ 1,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,
/* Row 23 */ 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
/* Row 24 */ 1,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,
/* Row 25 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* Row 26 */ 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,
/* Row 27 */ 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,
/* Row 28 */ 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,
/* Row 29 */ 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,
/* Row 30 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* Row 31 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

/* E1M1 player spawn */
#define E1M1_SPAWN_X  (15 * 256 + 128)
#define E1M1_SPAWN_Y  (28 * 256 + 128)

#include "../assets/doom/e1m2.h"
#include "../assets/doom/e1m3.h"
#include "../assets/doom/e1m4.h"

/* g_texture8 removed -- was unused and wasted 4KB of ROM */

#endif
