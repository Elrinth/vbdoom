#ifndef _FUNCTIONS_DOOR_H
#define _FUNCTIONS_DOOR_H

#include <types.h>
#include <stdbool.h>

/*
 * Door and switch system for VB Doom.
 * Doors are wall type 4 (WALL_TYPE_DOOR).
 * Switches are wall type 5 (WALL_TYPE_SWITCH).
 */

#define MAX_DOORS     24
#define MAX_SWITCHES  4

/* Door states */
#define DOOR_CLOSED   0
#define DOOR_OPENING  1
#define DOOR_OPEN     2
#define DOOR_CLOSING  3

/* Door timing (frames) */
#define DOOR_OPEN_SPEED   4    /* openAmount increments per frame */
#define DOOR_OPEN_MAX     64   /* fully open (64 = full texture height in texels) */
#define DOOR_STAY_TIME    100  /* frames to stay open before auto-close (5 sec @ 20fps) */

/* Switch types */
#define SW_DOOR   0   /* opens a linked door */
#define SW_EXIT   1   /* ends the level */

typedef struct {
    u8  tileX, tileY;
    u8  state;        /* DOOR_CLOSED .. DOOR_CLOSING */
    u8  openAmount;   /* 0=closed, DOOR_OPEN_MAX=fully open */
    u8  timer;        /* countdown for auto-close */
    u8  originalMap;  /* original g_map value (door wall type) */
} DoorState;

typedef struct {
    u8  tileX, tileY;
    u8  type;         /* SW_DOOR or SW_EXIT */
    u8  linkedDoor;   /* index into g_doors[] (for SW_DOOR type) */
    u8  activated;    /* 0=off, 1=on */
} SwitchState;

extern DoorState g_doors[MAX_DOORS];
extern SwitchState g_switches[MAX_SWITCHES];
extern u8 g_numDoors;
extern u8 g_numSwitches;
extern u8 g_levelComplete;  /* set to 1 when exit switch is hit */

/* Key state (set by key pickups, checked when opening key doors 9/10/11) */
extern u8 g_hasKeyRed;
extern u8 g_hasKeyYellow;
extern u8 g_hasKeyBlue;

/* Initialize doors and switches from the map data */
void initDoors(void);

/* Register a door/switch at a map position (call during level setup) */
void registerDoor(u8 tileX, u8 tileY);
void registerSwitch(u8 tileX, u8 tileY, u8 type, u8 linkedDoor);

/* Update all doors (animation, auto-close). Call once per frame. */
void updateDoors(void);

/* Player tries to activate something in front of them (Select button).
 * Checks adjacent tiles in facing direction within activation range.
 * Returns: 0 = nothing in range, 1 = activated door/switch,
 *          2 = hit non-interactive wall (play umf) */
u8 playerActivate(u16 playerX, u16 playerY, s16 playerA, u8 levelNum);

/* Find a registered door at a tile position. Returns index or -1. */
s8 findDoorAt(u8 tileX, u8 tileY);

/* Get door open amount for a tile position (0 if not a door or closed).
 * Used by raycaster to reduce wall height. */
u8 getDoorOpenAmount(u8 tileX, u8 tileY);

#endif
