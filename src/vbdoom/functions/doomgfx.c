#include <libgccvb.h>
#include <mem.h>
#include <video.h>
#include <world.h>
#include <constants.h>
#include <languages.h>
#include "doomgfx.h"
#include "enemy.h"

extern BYTE vb_doomTiles[];
extern BYTE vb_doomMap[];

void loadDoomGfxToMem() {
	//copymem((void*)(CharSeg0), (void*)vb_doomTiles, 13216);
	//copymem((void*)0x00078000, (void*)vb_doomTiles, 13216);
	copymem((void*)0x00078000, (void*)vb_doomTiles, 14224);
}

void loadEnemyFrame(u8 enemyIdx, const unsigned int* tileData) {
	/* Copy one frame's tile data into the char memory reserved for this enemy.
	 * Enemy 0 -> chars starting at ZOMBIE_CHAR_START (889)
	 * Enemy 1 -> chars starting at ZOMBIE_CHAR_START + ENEMY_CHAR_STRIDE (953)
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
u16 startPos; // not shooting
u16 startPosBlack;
u8 rowCount; // not shooting
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
	startPos = 0; // not shooting
	startPosBlack = 0;
	rowCount = 0; // not shooting
	curRow = 0;
	xTiles = 0;
	switch (iWeapon) {
		case 1: // fists

			if (iFrame == 0) { // fists not attacking
				xTiles = 30;
				lockedY = 146;
				startPos = 96*22;
				rowCount = 6;
				posX = 142;
			} else if (iFrame == 1) { // fists attack 1
				xTiles = 22; // 11*2
				startPos = (96*22) + 30;
				rowCount = 6;
				lockedY = 146;
				posX = 96;
			} else if (iFrame == 2) { // fists attack 2
				xTiles = 28; // 14*2
				startPos = (96*13) + 48;
				rowCount = 7;
				lockedY = 138;
				posX = 96; // vet ej...
			} else if (iFrame == 3) { // fists attack 3
				xTiles = 34; // 17 * 2
				startPos = (96*28);
				rowCount = 8;
				lockedY = 130;
				posX = 80;
			}
		break;
		case 2: // pistol
			xTiles = 16;
			posX = 160;
			blackY = 134;

			if (iFrame == 0) { // not shooting pos
				lockedY = 142;
				startPos = 96*9;
				startPosBlack = (96*28) + 34;
				rowCount = 7;
			} else if (iFrame == 1 || iFrame == 2 || iFrame == 3) { // shooting
				blackY = 128;
				startPos = (96*5) + 22;
				startPosBlack = (96*28) + 50;
				rowCount = 11;
				lockedY = 104;
			}
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
		WA[22].gx = posX;
	} else {
		WA[21].gy = lockedY+swayY;
		WA[21].gx = posX+swayX;

		// black weapon layer
		WA[22].gy = blackY+swayY;
		WA[22].gx = posX+swayX;
	}

	pos = 0;

	// only update graphics if there is any difference
	if (prevWeapon != iWeapon || prevFrame != iFrame) {
		// cleara med svart
		for (curRow = 0; curRow < 11; curRow++) {
			copymem((void*)BGMap(LAYER_WEAPON)+pos+(xTiles)+(curRow<<7), (void*)(vb_doomMap+0), 64);
		}
		for (curRow = 0; curRow < rowCount; curRow++) {
			copymem((void*)BGMap(LAYER_WEAPON)+pos+(curRow<<7), (void*)(vb_doomMap+startPos+(curRow*96)), xTiles);
		}
		if (xTiles < 34) { // clear the columns after...
			for (curRow = 0; curRow < rowCount; curRow++) {
				copymem((void*)BGMap(LAYER_WEAPON)+pos+(xTiles)+(curRow<<7), (void*)(vb_doomMap+0), (34-xTiles)*2);
			}
		}
		if (iWeapon == 2) { // dont draw the darker if fists

			for (curRow = 0; curRow < 8; curRow++) {
				copymem((void*)BGMap(LAYER_WEAPON_BLACK)+pos+(curRow<<7), (void*)(vb_doomMap+startPosBlack+(curRow*96)), xTiles);
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
		}
	}
}
u16 whichFace;
void drawDoomFace(u8 *face)
{
	switch(*face)
	{
		case 0:
			whichFace = 0;
			break;
		case 1:
			whichFace = 384;
			break;
		case 2:
			whichFace = 768;
			break;
	}
	copymem((void*)BGMap(LAYER_UI)+44, (void*)(vb_doomMap+400+whichFace), 6);
	copymem((void*)(BGMap(LAYER_UI)+172), (void*)(vb_doomMap+496+whichFace), 6);
	copymem((void*)(BGMap(LAYER_UI)+300), (void*)(vb_doomMap+592+whichFace), 6);
	copymem((void*)(BGMap(LAYER_UI)+428), (void*)(vb_doomMap+688+whichFace), 6);


	// font consists of the last 256 chars (1792-2047)


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