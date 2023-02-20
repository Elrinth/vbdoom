#ifndef _FUNCTIONS_DOOMGFX_H
#define _FUNCTIONS_DOOMGFX_H


#include <types.h>

void loadDoomGfxToMem();
void drawDoomFace(u8 bgmap, u16 x, u16 y, u8 face);
void drawDoomPistol(u8 bgmap, u16 x, u16 y, u8 pistolAnimation);
void drawDoomUI(u8 bgmap, u16 x, u16 y);

void drawBigUINumbers(u8 iType, u8 iOnes, u8 iTens, u8 iHundreds);
void drawWeapon(u8 iWeapon, u8 iFrame);
void drawUpdatedAmmo(u16 iAmmo, u8 iAmmoType);
void drawHealth(u16 iHealth);
void drawArmour(u16 iArmour);

#endif