#include <constants.h>
#include <controller.h>
#include <world.h>
#include <input.h>
#include <controller.h>
#include <text.h>
#include <languages.h>
#include "creditsScreen.h"
#include <virtualboy_pixeldraw.h>

extern BYTE CREDIT_idsoftwareTiles[];
extern unsigned short CREDIT_idsoftwareMap[];

extern BYTE CREDIT_virtualboyTiles[];
extern unsigned short CREDIT_virtualboyMap[];

void drawCredits(u8 iPart) {

	//copymem((void*)(CharSeg0), (void*)title_screenTiles, 13424);
	if (iPart == 0) {
		copymem(0x00078000, (void*)CREDIT_idsoftwareTiles, 14864);
	} else {
		copymem(0x00078000, (void*)CREDIT_virtualboyTiles, 13600);
	}


	u16 pos = 0; // 128 + 8;
	u16 i = 0;
	u16 y;
	u16 mapIndex;

	for (i = 0; i < 25; i++) {
		y = pos + i*128;
		mapIndex = i*40;
		if (iPart == 0) {
			copymem((void*)BGMap(0)+y, (void*)(CREDIT_idsoftwareMap+mapIndex), 80);
		} else {
			copymem((void*)BGMap(0)+y, (void*)(CREDIT_virtualboyMap+mapIndex), 80);
		}
	}
}

u8 creditsScreen()
{

	WA[31].head = WRLD_ON|WRLD_OVR;
	WA[31].w = 320;
	WA[31].h = 200;
	WA[30].head = WRLD_END;
/*
	vbSetWorld(31, 0xC000, 0, 0, 0, 0, 0, 0, 384, 224); // set up the Etch-A-Sketch board
	vbSetWorld(30, 0x0040, 0, 0, 0, 0, 0, 0, 0, 0); // Blank world and END bit set
*/
	vbDisplayOn(); // turns the display on

	//vbFXFadeIn(0);
	//vbWaitFrame(20);

	int count; // counter used for pauses

	//for (count=0; count < 200000; count++); // similar to waitframe, except not dependant on display
	u8 creditsPart = 0;
	drawCredits(creditsPart);

	//for (count=0; count < 200000; count++);
	//vbWaitFrame(73);
	vbDisplayShow();
	vbFXFadeIn(0);

	while(1) {
		if(buttonsPressed(K_STA|K_A|K_B|K_SEL, true)) {
			if (creditsPart == 0) {
				creditsPart++;
				drawCredits(creditsPart);
				for (count=0; count < 200000; count++);
				vbWaitFrame(23);
			}
			else {
				break;
			}
		}

		vbWaitFrame(0);
	}
	vbFXFadeOut(0);
	setmem((void*)BGMap(0), 0, 8192);
	vbDisplayHide();
	return 0;
}