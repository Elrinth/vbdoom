#include <libgccvb.h>
#include <mem.h>
#include <video.h>
#include <world.h>
#include <constants.h>
#include <languages.h>
#include "doomgfx.h"
#include "enemy.h"
#include "pickup.h"
#include "../assets/images/wall_textures.h"
#include "../assets/images/sprites/faces/face_sprites.h"
#include "../assets/images/fist_sprites.h"
#include "../assets/images/pistol_sprites.h"
#include "../assets/images/shotgun_sprites.h"

extern BYTE vb_doomTiles[];
extern BYTE vb_doomMap[];

void loadDoomGfxToMem() {
	/* Load HUD-only tileset (108 tiles, 1728 bytes) */
	copymem((void*)0x00078000, (void*)vb_doomTiles, 1728);
}

void loadFaceFrame(u8 faceIdx) {
	u32 addr = 0x00078000 + (u32)FACE_CHAR_START * 16;
	if (faceIdx >= FACE_COUNT) faceIdx = 0;
	copymem((void*)addr, (void*)FACE_TILE_DATA[faceIdx], FACE_TILE_BYTES);
}

void loadFistSprites(void) {
	u32 addr = 0x00078000 + (u32)WEAPON_CHAR_START * 16;
	copymem((void*)addr, (void*)fistTiles, FIST_TILE_BYTES);
}

void loadPistolSprites(void) {
	u32 addr = 0x00078000 + (u32)WEAPON_CHAR_START * 16;
	copymem((void*)addr, (void*)pistolTiles, PISTOL_TILE_BYTES);
}

void loadShotgunSprites(void) {
	u32 addr = 0x00078000 + (u32)WEAPON_CHAR_START * 16;
	copymem((void*)addr, (void*)shotgunTiles, SHOTGUN_TILE_BYTES);
}

void loadEnemyFrame(u8 enemyIdx, const unsigned int* tileData) {
	/* Copy one frame's tile data into the char memory reserved for this enemy.
	 * Enemy 0 -> chars starting at ZOMBIE_CHAR_START (691)
	 * Enemy 1 -> chars starting at ZOMBIE_CHAR_START + ENEMY_CHAR_STRIDE (755)
	 */
	u32 addr = 0x00078000 + (ZOMBIE_CHAR_START + enemyIdx * ENEMY_CHAR_STRIDE) * 16;
	copymem((void*)addr, (void*)tileData, ZOMBIE_FRAME_BYTES);
}

void initEnemyBGMaps() {
	/* Set up BGMap(3) and BGMap(4) with tile entries for enemy 0 and 1.
	 * Each enemy gets its own BGMap + char range.
	 * This only needs to be called once at init - animation is done by
	 * swapping tile data via loadEnemyFrame().
	 *
	 * BGMap layout: 64 entries per row (128 bytes per row).
	 * Each entry is u16: bits 0-10 = char number, bits 13-14 = palette.
	 * We use palette 0 (GPLT0 = normal shading).
	 */
	u8 e, row, col;
	for (e = 0; e < MAX_ENEMIES; e++) {
		u16 *bgmap = (u16*)BGMap(ENEMY_BGMAP_START + e);
		u16 charBase = ZOMBIE_CHAR_START + e * ENEMY_CHAR_STRIDE;
		for (row = 0; row < ZOMBIE_TILE_H; row++) {
			for (col = 0; col < ZOMBIE_TILE_W; col++) {
				u16 charIndex = charBase + row * ZOMBIE_TILE_W + col;
				bgmap[row * 64 + col] = charIndex;  /* palette 0, no flip */
			}
		}
	}
}

void loadPickupFrame(u8 pickupSlot, const unsigned int* tileData) {
	/* Copy one pickup's tile data into char memory.
	 * Pickup 0 -> chars starting at PICKUP_CHAR_START (883)
	 * Pickup 1 -> chars starting at PICKUP_CHAR_START + PICKUP_CHAR_STRIDE (895)
	 * etc.
	 */
	u32 addr = 0x00078000 + (PICKUP_CHAR_START + pickupSlot * PICKUP_CHAR_STRIDE) * 16;
	copymem((void*)addr, (void*)tileData, PICKUP_FRAME_BYTES);
}

void initPickupBGMaps(void) {
	/* Set up BGMap(6), (7), (8) with tile entries for pickup slots 0, 1, 2.
	 * Each pickup slot gets its own BGMap + char range.
	 * 4x3 tile grid (32x24 px).
	 */
	u8 p, row, col;
	for (p = 0; p < MAX_VISIBLE_PICKUPS; p++) {
		u16 *bgmap = (u16*)BGMap(PICKUP_BGMAP_START + p);
		u16 charBase = PICKUP_CHAR_START + p * PICKUP_CHAR_STRIDE;
		for (row = 0; row < PICKUP_TILE_H; row++) {
			for (col = 0; col < PICKUP_TILE_W; col++) {
				u16 charIndex = charBase + row * PICKUP_TILE_W + col;
				bgmap[row * 64 + col] = charIndex;  /* palette 0, no flip */
			}
		}
	}
}

void loadWallTextures(void) {
	/* Copy wall texture tiles into character memory at WALL_TEX_CHAR_START.
	 * 512 tiles * 16 bytes = 8192 bytes total. */
	u32 addr = 0x00078000 + (u32)WALL_TEX_CHAR_START * 16;
	copymem((void*)addr, (void*)wallTextureTiles, WALL_TEX_BYTES);
}


u16 drawPos;
u16 startPos;
u8 addForBlack;
void drawBigUINumbers(u8 iType, u8 iOnes, u8 iTens, u8 iHundreds, u8 iAmmoType) {

	drawPos = 3202-3072; // ammo
	switch (iType) {
		case 1: // health
			drawPos = 3216-3072;
		break;
		case 2: // armour
			drawPos = 3252-3072;
		break;
	}
	startPos = 96*16;
	addForBlack = 44;
	if (iHundreds > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+(iHundreds*4)), 4);
    	copymem((void*)BGMap(LAYER_UI)+drawPos+128, (void*)(vb_doomMap+startPos+(iHundreds*4)+96), 4);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+addForBlack), 4);
		copymem((void*)BGMap(LAYER_UI)+drawPos+128, (void*)(vb_doomMap+startPos+96+addForBlack), 4);
	}
	if (iHundreds > 0 || iTens > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+(iTens*4)), 4);
    	copymem((void*)BGMap(LAYER_UI)+drawPos+4+128, (void*)(vb_doomMap+startPos+96+(iTens*4)), 4);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+addForBlack), 4);
		copymem((void*)BGMap(LAYER_UI)+drawPos+4+128, (void*)(vb_doomMap+startPos+96+addForBlack), 4);
	}

	if (iAmmoType > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+8, (void*)(vb_doomMap+startPos+(iOnes*4)), 4);
    	copymem((void*)BGMap(LAYER_UI)+drawPos+8+128, (void*)(vb_doomMap+startPos+96+(iOnes*4)), 4);
	} else {
		// draw blank
		copymem((void*)BGMap(LAYER_UI)+drawPos+8, (void*)(vb_doomMap), 4);
		copymem((void*)BGMap(LAYER_UI)+drawPos+8+128, (void*)(vb_doomMap), 4);
	}


	if (iType > 0) { // add percentage
		copymem((void*)BGMap(LAYER_UI)+drawPos+12, (void*)(vb_doomMap+startPos+40), 4);
		copymem((void*)BGMap(LAYER_UI)+drawPos+12+128, (void*)(vb_doomMap+startPos+96+40), 4);
	}
}

u8 ones;
u8 tens;
u8 hundreds;

void drawHealth(u16 iHealth) {
	ones = iHealth % 10;
	tens = (iHealth / 10) % 10;
	hundreds = (iHealth / 100) % 10;
	drawBigUINumbers(1, ones, tens, hundreds, 1);
}
void drawArmour(u16 iArmour) {
	ones = iArmour % 10;
	tens = (iArmour / 10) % 10;
	hundreds = (iArmour / 100) % 10;
	drawBigUINumbers(2, ones, tens, hundreds, 1);
}

void drawUpdatedAmmo(u16 iAmmo, u8 iAmmoType) {
	ones = iAmmo % 10;
	tens = (iAmmo / 10) % 10;
	hundreds = (iAmmo / 100) % 10;
	drawBigUINumbers(0, ones, tens, hundreds,iAmmoType);

	if (iAmmoType > 0) {
		drawPos = 3280-3072;
		switch (iAmmoType) {
    		case 1: // bull
    			drawPos = 3280-128-3072;
    		break;
    		case 2: // shel
				drawPos = 3280-3072;
    		break;
    		case 3: // rckt
				drawPos = 3280+128-3072;
    		break;
    		case 4: // cell
				drawPos = 3280+256-3072;
    		break;
    	}
		startPos = 96*18;
		if (hundreds > 0) {
			copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
		} else {
			copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+20), 2); // blank?
		}
		if (hundreds > 0 || tens > 0) {
			copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+(tens*2)), 2);
		} else {
			copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+20), 2);
		}
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+(ones*2)), 2);
	}
}

u8 prevWeapon = 99;
u8 prevFrame = 99;
s16 prevSwayX = 0;
s16 prevSwayY = 0;
u8 prevWeaponChangeTimer = 0;
bool cancel;

u8 posX;
u16 pos;
u16 lockedY;
u16 blackY;
s8 blackXOff;
const void *weapMapPtr;
const void *weapBlkMapPtr;
u8 weapMapStride;
u8 weapBlkStride;
u8 rowCount;
u8 blkRowCount;
u8 curRow;
u8 xTiles;

void drawWeapon(u8 iWeapon, s16 swayX, s16 swayY, u8 iFrame, u8 iWeaponChangeTimer) {
	cancel = false;
	/*
	 // behöver ej göras då vi clearar med svart nedan
	if (iWeapon != prevWeapon) {
		setmem((void*)BGMap(LAYER_WEAPON_BLACK), 0, 1840);
		setmem((void*)BGMap(LAYER_WEAPON), 0, 1840);
	}*/
	if (iWeapon != prevWeapon) {
		if (iWeapon == 1)
			vbSetWorld(22, LAYER_WEAPON_BLACK, 	0, 0, 0, 0, 0, 0, 136, 128);
		else
			vbSetWorld(22, WRLD_ON|LAYER_WEAPON_BLACK, 	0, 0, 0, 0, 0, 0, 136, 128);
	}
	if (prevWeaponChangeTimer == iWeaponChangeTimer && prevFrame == iFrame && iWeapon == prevWeapon && prevSwayX == swayX && prevSwayY == swayY)
		cancel = true; // no need to do anything

	if (cancel) {
		prevWeaponChangeTimer = iWeaponChangeTimer;
		prevWeapon = iWeapon;
    	prevSwayX = swayX;
    	prevSwayY = swayY;
    	prevFrame = iFrame;
		return;
	}
	// reset



	posX = 0;
	pos = 0;
	lockedY = 0;
	blackY = 0;
	blackXOff = 0;
	weapMapPtr = (void*)0;
	weapBlkMapPtr = (void*)0;
	weapMapStride = 0;
	weapBlkStride = 0;
	rowCount = 0;
	blkRowCount = 0;
	curRow = 0;
	xTiles = 0;
	switch (iWeapon) {
		case 1: // fists
			if (iFrame == 0) {
				xTiles = 30;
				lockedY = 146;
				rowCount = 6;
				posX = 142;
				weapMapPtr = fistFrame0Map;
			} else if (iFrame == 1) {
				xTiles = 22;
				rowCount = 6;
				lockedY = 146;
				posX = 96;
				weapMapPtr = fistFrame1Map;
			} else if (iFrame == 2) {
				xTiles = 28;
				rowCount = 7;
				lockedY = 138;
				posX = 96;
				weapMapPtr = fistFrame2Map;
			} else if (iFrame == 3) {
				xTiles = 34;
				rowCount = 8;
				lockedY = 130;
				posX = 80;
				weapMapPtr = fistFrame3Map;
			}
			weapMapStride = xTiles;
		break;
		case 2: // pistol
			xTiles = 16;
			posX = 160;
			blackY = 134;
			weapMapStride = 16;
			weapBlkStride = 16;

			if (iFrame == 0) {
				lockedY = 142;
				rowCount = 7;
				weapMapPtr = pistolRedFrame0Map;
				weapBlkMapPtr = pistolBlackFrame0Map;
				blkRowCount = 8;
			} else if (iFrame == 1 || iFrame == 2 || iFrame == 3) {
				blackY = 128;
				rowCount = 11;
				lockedY = 104;
				weapMapPtr = pistolRedFrame1Map;
				weapBlkMapPtr = pistolBlackFrame1Map;
				blkRowCount = 8;
			}
		break;
		case 3: // shotgun (6 frames: idle, fire1, fire2, pump1, pump2, pump3)
			posX = 152;
			if (iFrame == 0) {
				xTiles = SHOTGUN_F0_XTILES;
				rowCount = SHOTGUN_F0_ROWS;
				lockedY = 192 - SHOTGUN_F0_ROWS * 8;
				weapMapPtr = shotgunFrame0Map;
				weapMapStride = SHOTGUN_F0_XTILES;
			} else if (iFrame == 1) {
				xTiles = SHOTGUN_F1_XTILES;
				rowCount = SHOTGUN_F1_ROWS;
				lockedY = 192 - SHOTGUN_F1_ROWS * 8;
				weapMapPtr = shotgunFrame1Map;
				weapMapStride = SHOTGUN_F1_XTILES;
			} else if (iFrame == 2) {
				xTiles = SHOTGUN_F2_XTILES;
				rowCount = SHOTGUN_F2_ROWS;
				lockedY = 192 - SHOTGUN_F2_ROWS * 8;
				weapMapPtr = shotgunFrame2Map;
				weapMapStride = SHOTGUN_F2_XTILES;
			} else if (iFrame == 3) {
				xTiles = SHOTGUN_F3_XTILES;
				rowCount = SHOTGUN_F3_ROWS;
				lockedY = 192 - SHOTGUN_F3_ROWS * 8;
				weapMapPtr = shotgunFrame3Map;
				weapMapStride = SHOTGUN_F3_XTILES;
			} else if (iFrame == 4) {
				xTiles = SHOTGUN_F4_XTILES;
				rowCount = SHOTGUN_F4_ROWS;
				lockedY = 192 - SHOTGUN_F4_ROWS * 8;
				weapMapPtr = shotgunFrame4Map;
				weapMapStride = SHOTGUN_F4_XTILES;
			} else if (iFrame == 5) {
				xTiles = SHOTGUN_F5_XTILES;
				rowCount = SHOTGUN_F5_ROWS;
				lockedY = 192 - SHOTGUN_F5_ROWS * 8;
				weapMapPtr = shotgunFrame5Map;
				weapMapStride = SHOTGUN_F5_XTILES;
			}
			/* Black layer: same maps, offset down+right for shadow/outline effect */
			weapBlkMapPtr = weapMapPtr;
			weapBlkStride = weapMapStride;
			blkRowCount = rowCount;
			blackY = lockedY + 1;
			blackXOff = 1;
		break;
		default:
		break;
	}
	if (iWeaponChangeTimer > 0) {

		if (iWeaponChangeTimer > 10) {
			lockedY += 80 - (iWeaponChangeTimer<<2);
			blackY += 80 - (iWeaponChangeTimer<<2);
		} else {
			lockedY += (iWeaponChangeTimer<<2);
			blackY += (iWeaponChangeTimer<<2);
		}
	}
	if (iFrame > 0) { // no sway during shooting...
		WA[21].gy = lockedY;
		WA[21].gx = posX;

		// black weapon layer
		WA[22].gy = blackY;
		WA[22].gx = posX + blackXOff;
	} else {
		WA[21].gy = lockedY+swayY;
		WA[21].gx = posX+swayX;

		// black weapon layer
		WA[22].gy = blackY+swayY;
		WA[22].gx = posX+swayX + blackXOff;
	}

	pos = 0;

	/* only update graphics if there is any difference */
	if (prevWeapon != iWeapon || prevFrame != iFrame) {
		/* clear weapon layer with blank tiles */
		for (curRow = 0; curRow < 11; curRow++) {
			copymem((void*)BGMap(LAYER_WEAPON)+pos+(xTiles)+(curRow<<7), (void*)(vb_doomMap+0), 64);
		}
		/* draw weapon from compact map (stride = xTiles per row) */
		for (curRow = 0; curRow < rowCount; curRow++) {
			copymem((void*)BGMap(LAYER_WEAPON)+pos+(curRow<<7), (BYTE*)weapMapPtr + curRow * weapMapStride, xTiles);
		}
		if (xTiles < 34) { /* clear columns after weapon */
			for (curRow = 0; curRow < rowCount; curRow++) {
				copymem((void*)BGMap(LAYER_WEAPON)+pos+(xTiles)+(curRow<<7), (void*)(vb_doomMap+0), (34-xTiles)*2);
			}
		}
		/* clear black layer */
		for (curRow = 0; curRow < 11; curRow++) {
			copymem((void*)BGMap(LAYER_WEAPON_BLACK)+pos+(curRow<<7), (void*)(vb_doomMap+0), 64);
		}
		if (weapBlkMapPtr) {
			for (curRow = 0; curRow < blkRowCount; curRow++) {
				copymem((void*)BGMap(LAYER_WEAPON_BLACK)+pos+(curRow<<7), (BYTE*)weapBlkMapPtr + curRow * weapBlkStride, xTiles);
			}
			if (xTiles < 34) { /* clear columns after black weapon */
				for (curRow = 0; curRow < blkRowCount; curRow++) {
					copymem((void*)BGMap(LAYER_WEAPON_BLACK)+pos+(xTiles)+(curRow<<7), (void*)(vb_doomMap+0), (34-xTiles)*2);
				}
			}

			u8 xx,yy;
			for (yy = 0; yy < 46; yy++) {
				for (xx = 0; xx < 40; xx++) {
					BGM_PALSET(LAYER_WEAPON_BLACK, xx, yy, BGM_PAL2);
				}
			}
		}
	}
	prevWeaponChangeTimer = iWeaponChangeTimer;
	prevWeapon = iWeapon;
	prevSwayX = swayX;
	prevSwayY = swayY;
	prevFrame = iFrame;
}

const unsigned short test[2] __attribute__((aligned(4)))=
{
	0x4011,0x4011
};
u16 j;
void drawDoomUI(u8 bgmap, u16 x, u16 y) {
	copymem((void*)BGMap(bgmap), (void*)(vb_doomMap), 96);
	copymem((void*)BGMap(bgmap)+128, (void*)(vb_doomMap+96), 96);
	copymem((void*)BGMap(bgmap)+256, (void*)(vb_doomMap+192), 96);
	copymem((void*)BGMap(bgmap)+384, (void*)(vb_doomMap+288), 96);

	for (j = 0; j < 128; j+=2) {
		copymem((void*)BGMap(LAYER_UI_BLACK)+j, (void*)(vb_doomMap+384), 2);
		copymem((void*)BGMap(LAYER_UI_BLACK)+128+j, (void*)(vb_doomMap+384), 2);
		copymem((void*)BGMap(LAYER_UI_BLACK)+256+j, (void*)(vb_doomMap+384), 2);
		copymem((void*)BGMap(LAYER_UI_BLACK)+384+j, (void*)(vb_doomMap+384), 2);
	}
	// should only need to do this ONCE actually and never again TBH...
	for (y = 0; y < 24; y++) {
		for (x = 0; x < 48; x++) {
			BGM_PALSET(12, x, y, BGM_PAL1);
			BGM_PALSET(LAYER_UI, x, y, BGM_PAL0);
		}
	}
}
void drawDoomFace(u8 *face)
{
	/* Fixed faceMap: always references chars 108-119 (12 entries, 24 bytes).
	 * Tile data is loaded dynamically by loadFaceFrame(). */
	copymem((void*)BGMap(LAYER_UI)+44, (BYTE*)faceMap, 6);
	copymem((void*)(BGMap(LAYER_UI)+172), (BYTE*)faceMap + 6, 6);
	copymem((void*)(BGMap(LAYER_UI)+300), (BYTE*)faceMap + 12, 6);
	copymem((void*)(BGMap(LAYER_UI)+428), (BYTE*)faceMap + 18, 6);
}
void drawPlayerInfo(u16 *fPlayerX, u16 *fPlayerY, s16 *fPlayerAng) {

	u8 ones = *fPlayerX % 10;
	u8 tens = (*fPlayerX / 10) % 10;
	u8 hundreds = (*fPlayerX / 100) % 10;
	u8 thousands = (*fPlayerX / 1000) % 10;
	u8 tenthousands = (*fPlayerX / 10000) % 10;

	drawPos = 0;

	startPos = 96*18;

	if (tenthousands > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+(tenthousands*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}

	if (tenthousands > 0 || thousands > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+(thousands*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}

	if (tenthousands > 0 || thousands > 0 || hundreds > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}
	if (tenthousands > 0 || thousands > 0 || hundreds > 0 || tens > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+6, (void*)(vb_doomMap+startPos+(tens*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+6, (void*)(vb_doomMap+startPos+20), 2);
	}
	copymem((void*)BGMap(LAYER_UI)+drawPos+8, (void*)(vb_doomMap+startPos+(ones*2)), 2);

	ones = *fPlayerY % 10;
	tens = (*fPlayerY / 10) % 10;
	hundreds = (*fPlayerY / 100) % 10;
	thousands = (*fPlayerY / 1000) % 10;
	tenthousands = (*fPlayerY / 10000) % 10;

	drawPos = 16;

	startPos = 96*18;

	if (tenthousands > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+(tenthousands*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}

	if (tenthousands > 0 || thousands > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+(thousands*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}

	if (tenthousands > 0 || thousands > 0 || hundreds > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}
	if (tenthousands > 0 || hundreds > 0 || tens > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+6, (void*)(vb_doomMap+startPos+(tens*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+6, (void*)(vb_doomMap+startPos+20), 2);
	}
	copymem((void*)BGMap(LAYER_UI)+drawPos+8, (void*)(vb_doomMap+startPos+(ones*2)), 2);

	ones = *fPlayerAng % 10;
	tens = (*fPlayerAng / 10) % 10;
	hundreds = (*fPlayerAng / 100) % 10;
	thousands = (*fPlayerAng / 1000) % 10;

	drawPos = 34;

	if (thousands > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+(thousands*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+2, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}

	if (thousands > 0 || hundreds > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+4, (void*)(vb_doomMap+startPos+20), 2); // blank?
	}
	if (thousands > 0 || hundreds > 0 || tens > 0) {
		copymem((void*)BGMap(LAYER_UI)+drawPos+6, (void*)(vb_doomMap+startPos+(tens*2)), 2);
	} else {
		copymem((void*)BGMap(LAYER_UI)+drawPos+6, (void*)(vb_doomMap+startPos+20), 2);
	}
	copymem((void*)BGMap(LAYER_UI)+drawPos+8, (void*)(vb_doomMap+startPos+(ones*2)), 2);
}