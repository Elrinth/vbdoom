#ifndef _FUNCTIONS_SAVEGAME_H
#define _FUNCTIONS_SAVEGAME_H

#include <types.h>
#include <stdbool.h>

/*
 * Save game to battery-backed SRAM (SAVERAM at 0x06000000).
 * SaveState is filled by the game loop and written by saveGameToSRAM when
 * the user selects SAVE GAME in the pause menu.
 */

#define SAVE_MAX_ENEMIES  21
#define SAVE_MAX_PICKUPS  16
#define SAVE_MAGIC        0x5642  /* "VB" */
#define SAVE_VERSION      1

typedef struct SaveWeapon {
	u8  hasWeapon;
	u8  ammo;
} SaveWeapon;

typedef struct SavePickup {
	u16 x;
	u16 y;
	u8  type;
	u8  animFrame;
	u8  animTimer;
	u8  active;
	u8  respawnTimer;
} SavePickup;

typedef struct SaveEnemy {
	u16  x;
	u16  y;
	s16  angle;
	u8   state;
	u8   health;
	u8   movedir;
	u8   enemyType;
	u8   active;
} SaveEnemy;

typedef struct SaveState {
	u8  level;
	u16 playerX;
	u16 playerY;
	s16 playerAngle;
	u8  health;
	u16 armour;
	u8  armorType;
	u8  hasKeyRed;
	u8  hasKeyYellow;
	u8  hasKeyBlue;
	u8  currentWeapon;
	SaveWeapon  weapons[6];
	SavePickup  pickups[SAVE_MAX_PICKUPS];
	SaveEnemy   enemies[SAVE_MAX_ENEMIES];
} SaveState;

/* Write SaveState to SRAM. Safe to call from options screen. */
void saveGameToSRAM(const SaveState *s);

/* Read save from SRAM into caller buffer. Returns true if magic/version valid. */
bool loadGameFromSRAM(SaveState *s);

/* Load into internal buffer (for title → game flow). Returns true if valid save. */
bool tryLoadGameFromSRAM(void);
/* Return non-NULL if tryLoadGameFromSRAM() succeeded; gameLoop applies then calls clearLoadedSave(). */
const SaveState *getLoadedSave(void);
void clearLoadedSave(void);

#endif
