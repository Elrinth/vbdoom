#ifndef _FUNCTIONS_DOOMGFX_H
#define _FUNCTIONS_DOOMGFX_H

#include <stdbool.h>

#include <types.h>

/* Weapon sprite character memory (shared between fists, pistol, shotgun) */
#define WEAPON_CHAR_START  544

/* Zombie sprite character memory (after particles) */
#define ZOMBIE_CHAR_START  983
#define ZOMBIE_TILE_W  8
#define ZOMBIE_TILE_H  8
#define ZOMBIE_FRAME_BYTES  1024   /* 64 tiles * 16 bytes each */
#define ZOMBIE_PX_W  64
#define ZOMBIE_PX_H  64

/* Multi-enemy: each enemy gets 64 chars and its own BGMap */
#define ENEMY_CHAR_STRIDE  64
#define ENEMY_BGMAP_START  3   /* BGMap(3) for enemy 0, BGMap(4) for enemy 1 */

void loadDoomGfxToMem();
void loadEnemyFrame(u8 enemyIdx, const unsigned int* tileData);
void initEnemyBGMaps(void);

/* Pickup sprite support */
void loadPickupFrame(u8 pickupSlot, const unsigned int* tileData);
void initPickupBGMaps(void);

/* Wall texture tile loading */
void loadWallTextures(void);

/* Extracted sprite loading */
void loadFaceFrame(u8 faceIdx);
void loadFistSprites(void);
void loadPistolSprites(void);
void loadShotgunSprites(void);
void loadRocketLauncherSprites(void);

void drawDoomFace(u8 *face);
void drawDoomPistol(u8 bgmap, u16 x, u16 y, u8 pistolAnimation);
void drawDoomUI(u8 bgmap, u16 x, u16 y);

void drawBigUINumbers(u8 iType, u8 iOnes, u8 iTens, u8 iHundreds, u8 iAmmoType);
void drawWeapon(u8 iWeapon, s16 swayX, s16 swayY, u8 iFrame, u8 iWeaponChangeTimer);
void drawUpdatedAmmo(u16 iAmmo, u8 iAmmoType);
void drawSmallAmmo(u16 iAmmo, u8 iAmmoType);
void drawHealth(u16 iHealth);
void drawArmour(u16 iArmour);

void drawPlayerInfo(u16 *fPlayerX, u16 *fPlayerY, s16 *fPlayerAng);

/* Draw Doom slot numbers (2,3,4 top; 5,6,7 bottom) for owned weapons, left of face */
void drawWeaponSlotNumbers(u8 hasPistol, u8 hasShotgun, u8 hasRocket, u8 currentWeapon);

/* Update weapon HUD highlight: brighten active weapon's ammo type text + rect */
void highlightWeaponHUD(u8 weaponIdx);

#endif