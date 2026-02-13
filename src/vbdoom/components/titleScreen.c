#include <constants.h>
#include <controller.h>
#include <world.h>
#include <input.h>
#include <controller.h>
#include <text.h>
#include <languages.h>
#include "titleScreen.h"
#include <virtualboy_pixeldraw.h>
#include "../functions/sndplay.h"
#include "../assets/audio/doom_sfx.h"

extern BYTE title_screenTiles[];
extern unsigned short title_screenMap[];

//extern BYTE title_screen_optionsTiles[];
//extern unsigned short title_screen_optionsMap[];

extern BYTE selection_headTiles[];
extern unsigned short selection_headMap[];

void drawSkull(u8 index, u16 posY) {

	u16 mapIndex = 25*40;

	WA[29].gy = WA[30].gy = WA[31].gy + 118+posY*16;
	WA[29].gx = WA[30].gx = WA[31].gx + 82;

	WA[28].gy = WA[29].gy+4;
	WA[28].gx = WA[30].gx+5;

	// draw skull in second layer
	copymem((void*)BGMap(1), (void*)(title_screenMap+mapIndex), 6);
	copymem((void*)BGMap(1)+128, (void*)(title_screenMap+mapIndex+3), 6);

	copymem((void*)BGMap(2), (void*)(title_screenMap+mapIndex+6), 6);
	copymem((void*)BGMap(2)+128, (void*)(title_screenMap+mapIndex+9), 6);
	u8 xx,yy;

	for (yy = 0; yy < 46; yy++) {
		for (xx = 0; xx < 40; xx++) {
			BGM_PALSET(2, xx, yy, BGM_PAL2);
		}
	}

	//copymem((void*)BGMap(0)+pos+256, (void*)(title_screen_optionsMap+mapIndex+4), 2); //eyes
	copymem((void*)BGMap(3), (void*)(title_screenMap+mapIndex+12+index), 2); //eyes
	//copymem((void*)BGMap(0)+128, (void*)(title_screen_optionsTiles+mapIndex), 8);
}
void drawDoomTitle(u8 iOption) {

	//copymem((void*)(CharSeg0), (void*)title_screenTiles, 13424);
	/*if (iOption == 0) {
		copymem(0x00078000, (void*)title_screenTiles, 15872);
	} else {*/
		 //16064);
	//}


	u16 pos = 0; //128 + 8;
	u16 i = 0;
	u16 y;
	u16 mapIndex;
	//copymem((void*)BGMap(0), (void*)(title_screen_optionsMap), 15872);


	for (i = 0; i < 25; i++) {
		y = pos + i*128;
		mapIndex = i*40;
		copymem((void*)BGMap(0)+y, (void*)(title_screenMap+mapIndex), 80);
	}
}

/* Clear BGMap(0) border so 320x200 content doesn't show garbage when returning from options (real hardware). */
static void clearTitleBorder(void) {
	u16 row;
	for (row = 0; row < 25; row++) {
		setmem((void*)BGMap(0) + row * 128 + 80, 0, 48);
	}
	for (row = 25; row < 32; row++) {
		setmem((void*)BGMap(0) + row * 128, 0, 128);
	}
}

u8 titleScreen()
{
	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192); /* clear previous data */
	vbSetWorld(31, WRLD_ON|0, 0, 0, 0, 0, 0, 0, 320, 200);
	vbSetWorld(30, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 32, 32); // skull
	vbSetWorld(29, WRLD_ON|2, 0, 0, 0, 0, 0, 0, 32, 32); // skull blacks
	vbSetWorld(28, WRLD_ON|3, 0, 0, 0, 0, 0, 0, 8, 8); // eyes
	WA[26].head = WRLD_END;

	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192); /* clear previous data */
	vbDisplayOn(); // turns the display on


	WA[31].gy = 12;
	WA[31].gx = (384-320)>>1;

	VIP_REGS[GPLT0] = 0xE4;	/* Set 1st palettes to: 11100100 */
	VIP_REGS[GPLT1] = 0x00;	/* (i.e. "Normal" dark to light progression.) */
	VIP_REGS[GPLT2] = 0x84; // VIP_REGS[GPLT2] = 0xC2;

	VIP_REGS[GPLT3] = 0xA2;

	//vbFXFadeIn(0);
	//vbWaitFrame(20);

	copymem((void*)0x00078000, (void*)title_screenTiles, 9712);  /* 607 tiles * 16 bytes */

	drawDoomTitle(1);
	clearTitleBorder();
	vbDisplayShow(); // shows the display

	/* Only fade in on the very first visit (cold boot).
	 * When returning from options/credits the screen is already visible. */
	static u8 firstTime = 1;
	if (firstTime) {
		vbFXFadeIn(0);
		firstTime = 0;
	}

	u8 pos = 0;
	u8 skullAnim = 0;
	u8 count = 0;
	u8 countSkullAnimUp = 1;
	u8 pushedButton = 0;
	u8 pushedButtonCounter = 0;
	u8 allowPushAgain = 0;

	u16 keyInputs;

	while(1) {
		keyInputs = vbReadPad();
		if(keyInputs & (K_STA|K_A|K_B|K_SEL)) {
			if (allowPushAgain == 1 && pos != 1) {  /* pos 1=Load Game (disabled) */
				playPlayerSFX(SFX_PISTOL);
				break;
			}
		} else if(keyInputs & (K_RU|K_LU)) {
			if (allowPushAgain == 1) {
				allowPushAgain = 0;
				if (pos > 0) {
					pos--;
					playPlayerSFX(SFX_ELEVATOR_STP);
				}
			}
		} else if(keyInputs & (K_RD|K_LD)) {
			if (allowPushAgain == 1) {
				allowPushAgain = 0;
				if (pos < 4) {
					pos++;
					playPlayerSFX(SFX_ELEVATOR_STP);
				}
			}
		} else { // let go of all buttons

			allowPushAgain = 1;
		}

		drawSkull(skullAnim, pos);
		drawDoomTitle(1);
		clearTitleBorder();

		if (count > 6) {
			count = 0;
			if (countSkullAnimUp == 1) {
				skullAnim++;
				if (skullAnim > 2) {
					countSkullAnimUp = 0;
					skullAnim = 1;
				}
			} else {
				if (skullAnim == 0) {
					countSkullAnimUp = 1;
					skullAnim++;
				} else {
					skullAnim--;
				}
			}
		}
		if (pushedButton) {
			pushedButtonCounter++;
			if (pushedButtonCounter > allowPushAgain) {
				pushedButton = 0;
				pushedButtonCounter = 0;
			}
		}
		/* Tick the music sequencer so title music plays */
		updateMusic(true);

		count++;
		vbWaitFrame(0);
	}
	if (pos == 0) {
		vbFXFadeOut(0);
	}
	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192); /* clear previous data */
	return (pos+1);
}