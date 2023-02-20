#include <libgccvb.h>
#include <mem.h>
#include <video.h>
#include <world.h>
#include <constants.h>
#include <languages.h>
#include "doomgfx.h"

extern BYTE vb_doomTiles[];
extern BYTE vb_doomMap[];

void loadDoomGfxToMem() {
	//copymem((void*)(CharSeg0), (void*)vb_doomTiles, 13216);
	//copymem((void*)0x00078000, (void*)vb_doomTiles, 13216);
	copymem((void*)0x00078000, (void*)vb_doomTiles, 14224);

}

void drawBigUINumbers(u8 iType, u8 iOnes, u8 iTens, u8 iHundreds) {

	u16 drawPos = 3202-3072; // ammo
	switch (iType) {
		case 1: // health
			drawPos = 3216-3072;
		break;
		case 2: // armour
			drawPos = 3252-3072;
		break;
	}
	u16 startPos = 96*16;
	u8 bgmap = LAYER_UI;
	u8 addForBlack = 44;
	if (iHundreds > 0) {
		copymem((void*)BGMap(bgmap)+drawPos, (void*)(vb_doomMap+startPos+(iHundreds*4)), 4);
    	copymem((void*)BGMap(bgmap)+drawPos+128, (void*)(vb_doomMap+startPos+(iHundreds*4)+96), 4);
	} else {
		copymem((void*)BGMap(bgmap)+drawPos, (void*)(vb_doomMap+startPos+addForBlack), 4);
		copymem((void*)BGMap(bgmap)+drawPos+128, (void*)(vb_doomMap+startPos+96+addForBlack), 4);
	}
	if (iHundreds > 0 || iTens > 0) {
		copymem((void*)BGMap(bgmap)+drawPos+4, (void*)(vb_doomMap+startPos+(iTens*4)), 4);
    	copymem((void*)BGMap(bgmap)+drawPos+4+128, (void*)(vb_doomMap+startPos+96+(iTens*4)), 4);
	} else {
		copymem((void*)BGMap(bgmap)+drawPos+4, (void*)(vb_doomMap+startPos+addForBlack), 4);
		copymem((void*)BGMap(bgmap)+drawPos+4+128, (void*)(vb_doomMap+startPos+96+addForBlack), 4);
	}

	copymem((void*)BGMap(bgmap)+drawPos+8, (void*)(vb_doomMap+startPos+(iOnes*4)), 4);
	copymem((void*)BGMap(bgmap)+drawPos+8+128, (void*)(vb_doomMap+startPos+96+(iOnes*4)), 4);

	if (iType > 0) { // add percentage
		copymem((void*)BGMap(bgmap)+drawPos+12, (void*)(vb_doomMap+startPos+40), 4);
		copymem((void*)BGMap(bgmap)+drawPos+12+128, (void*)(vb_doomMap+startPos+96+40), 4);
	}
}

void drawHealth(u16 iHealth) {
	u8 ones = iHealth % 10;
	u8 tens = (iHealth / 10) % 10;
	u8 hundreds = (iHealth / 100) % 10;
	drawBigUINumbers(1, ones, tens, hundreds);
}
void drawArmour(u16 iArmour) {
	u8 ones = iArmour % 10;
	u8 tens = (iArmour / 10) % 10;
	u8 hundreds = (iArmour / 100) % 10;
	drawBigUINumbers(2, ones, tens, hundreds);
}

void drawUpdatedAmmo(u16 iAmmo, u8 iAmmoType) {
	u8 ones = iAmmo % 10;
	u8 tens = (iAmmo / 10) % 10;
	u8 hundreds = (iAmmo / 100) % 10;
	drawBigUINumbers(0, ones, tens, hundreds);
	u8 bgmap = LAYER_UI;

	if (iAmmoType > 0) {
		u16 drawPos = 3280-3072;
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
		u16 startPos = 96*18;
		if (hundreds > 0) {
			copymem((void*)BGMap(bgmap)+drawPos, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
		} else {
			copymem((void*)BGMap(bgmap)+drawPos, (void*)(vb_doomMap+startPos+40), 2);
		}
		if (hundreds > 0 || tens > 0) {
			copymem((void*)BGMap(bgmap)+drawPos+2, (void*)(vb_doomMap+startPos+(tens*2)), 2);
		} else {
			copymem((void*)BGMap(bgmap)+drawPos+2, (void*)(vb_doomMap+startPos+40), 2);
		}
		copymem((void*)BGMap(bgmap)+drawPos+4, (void*)(vb_doomMap+startPos+(ones*2)), 2);
	}
}

void drawWeapon(u8 iWeapon, u8 iFrame) {
	u8 posX = 0;
	u16 pos = 0;
	u16 lockedY = 0;
	u16 blackY = 0;
	u16 startPos = 0; // not shooting
	u16 startPosBlack = 0;
	u8 colCount = 0; // not shooting
	u8 curCol = 0;
	u8 xTiles = 0;
	switch (iWeapon) {
		case 1: // fists

			if (iFrame == 0) { // not attacking pos
				xTiles = 30;
				lockedY = 146;
				startPos = 96*22;
				colCount = 6;
				posX = 142;
			} else if (iFrame == 1) { // shooting
				xTiles = 24;
				startPos = (96*22) + 30;
				colCount = 6;
				lockedY = 146;
				posX = 96;
			} else if (iFrame == 2) { // shooting 2
			} else if (iFrame == 3) { // shooting 2

			}
		break;
		case 2: // pistol
			xTiles = 16;
			posX = 160;
			blackY = 134;

			if (iFrame == 0) { // not shooting pos
				lockedY = 142;
				startPos = 96*9;
				startPosBlack = startPos + (96*29);
				colCount = 7;
			} else if (iFrame == 1 || iFrame == 2 || iFrame == 3) { // shooting
				blackY = 128;
				startPos = (96*5) + 22;
				startPosBlack = 96*9  + 22 + (96*29);
				colCount = 11;
				lockedY = 104;
			}
		break;
		default:
		break;
	}
	WA[21].gy = lockedY;
	WA[21].gx = posX;

	// black weapon layer
	WA[22].gy = blackY;
	WA[22].gx = posX;

	pos = 0;

	for (curCol = 0; curCol < colCount; curCol++) {
		copymem((void*)BGMap(LAYER_WEAPON)+pos+(curCol*128), (void*)(vb_doomMap+startPos+(curCol*96)), xTiles);
	}
	if (iWeapon != 1) { // dont draw the darker, if black...
		for (curCol = 0; curCol < 8; curCol++) {
			copymem((void*)BGMap(LAYER_WEAPON_BLACK)+pos+(curCol*128), (void*)(vb_doomMap+startPosBlack+(curCol*96)), xTiles);
		}

		u8 x,y;
		for (y = 0; y < 46; y++) {
			for (x = 0; x < 40; x++) {
				BGM_PALSET(LAYER_WEAPON_BLACK, x, y, BGM_PAL2);
			}
		}
	}
}

const unsigned short test[2] __attribute__((aligned(4)))=
{
	0x4011,0x4011
};

void drawDoomUI(u8 bgmap, u16 x, u16 y) {
	copymem((void*)BGMap(bgmap), (void*)(vb_doomMap), 96);
	copymem((void*)BGMap(bgmap)+128, (void*)(vb_doomMap+96), 96);
	copymem((void*)BGMap(bgmap)+256, (void*)(vb_doomMap+96+96), 96);
	copymem((void*)BGMap(bgmap)+384, (void*)(vb_doomMap+96+96+96), 96);

	u16 j;
	for (j = 0; j < 128; j+=2) {
	/*
		copymem((void*)BGMap(12)+pos+j, (void*)(test), 2);
		copymem((void*)BGMap(12)+pos+128+j, (void*)(test), 2);
		copymem((void*)BGMap(12)+pos+128+128+j, (void*)(test), 2);
		copymem((void*)BGMap(12)+pos+128+128+128+j, (void*)(test), 2);*/

		copymem((void*)BGMap(LAYER_UI_BLACK)+j, (void*)(vb_doomMap+96+96+96+96), 2);
		copymem((void*)BGMap(LAYER_UI_BLACK)+128+j, (void*)(vb_doomMap+96+96+96+96), 2);
		copymem((void*)BGMap(LAYER_UI_BLACK)+128+128+j, (void*)(vb_doomMap+96+96+96+96), 2);
		copymem((void*)BGMap(LAYER_UI_BLACK)+128+128+128+j, (void*)(vb_doomMap+96+96+96+96), 2);
	}
	// should only need to do this ONCE actually and never again TBH...
	for (y = 0; y < 24; y++) {
		for (x = 0; x < 48; x++) {
			BGM_PALSET(12, x, y, BGM_PAL1);
		}
	}
}

void drawDoomFace(u8 bgmap, u16 x, u16 y, u8 face)
{
	//copymem((void*)CharSeg0, (void*)ffTiles, 256);
	//copymem((void*)BGMap(0), (void*)ffMapvb, 10);

	//copymem((void*)(CharSeg0), (void*)ffTiles, 256);
	//copymem((void*)BGMap(0), (void*)ffMap, 40);
	u16 pos=0;
	pos = (y << 7) + x;

	u16 whichFace = (face*(96+96+96+96));
	u16 startPos = 96+96+96+96+16;

	copymem((void*)BGMap(bgmap)+pos, (void*)(vb_doomMap+startPos+whichFace), 6);
	copymem((void*)(BGMap(bgmap)+pos+128), (void*)(vb_doomMap+startPos+96+whichFace), 6);
	copymem((void*)(BGMap(bgmap)+pos+256), (void*)(vb_doomMap+startPos+96+96+whichFace), 6);
	copymem((void*)(BGMap(bgmap)+pos+384), (void*)(vb_doomMap+startPos+96+96+96+whichFace), 6);

/*
	copymem((void*)BGMap(0x1000 * bgmap)+pos, 0, 8);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+128), 0+8, 8);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+256), 0+16, 8);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+384), 0+24, 8);*/


	// font consists of the last 256 chars (1792-2047)


}