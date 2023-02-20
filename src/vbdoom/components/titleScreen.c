#include <constants.h>
#include <controller.h>
#include <world.h>
#include <input.h>
#include <controller.h>
#include <text.h>
#include <languages.h>
#include "titleScreen.h"
#include <virtualboy_pixeldraw.h>

extern BYTE title_screenTiles[];
extern unsigned short title_screenMap[];

extern BYTE title_screen_optionsTiles[];
extern unsigned short title_screen_optionsMap[];

extern BYTE selection_headTiles[];
extern unsigned short selection_headMap[];

//extern unsigned int title_screen_384x224Tiles[];
//extern unsigned short title_screen_384x224Map[];
/*
void drawDoomImage() {
	u16 x = 0;
	u16 y = 0;
	u16 w = 0;
	u16 h = 0;
	u16 i = 0;
	u16 j = 0;
	u8 shade = 3;
	u8 parallax = 0;
	u8 height = 8;
	u8 width = 8;
	unsigned int pixelData;
	u8 byteArr[] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};

    for (i = 0; i < 5376; i+=4) {

    	for (j = 0; j < 4; j++) {
    		pixelData = title_screen_384x224Tiles[i+j];
    		byteArr[0+j*4] = (pixelData >> 24) & 0x000000FF;
			byteArr[1+j*4] = (pixelData >> 16) & 0x000000FF;
			byteArr[2+j*4] = (pixelData >>  8) & 0x000000FF;
			byteArr[3+j*4] = (pixelData >>  0) & 0x000000FF;
    	}

    	x = (i*2)%384;
		y = ((i*2)/384)*8;

		for (h = 0; h < height; h++) {
        	for (w = 0; w < width; w++) {
				u8 hiBit = (byteArr[h*2 + 1] >> (7-w)) & 1;
				u8 loBit = (byteArr[h*2 + 0] >> (7-w)) & 1;
		    	shade = (hiBit << 1) | loBit;
			    // draws pixel with buffhln (more efficient than buffvln or buffln) with length of 0 (so it's only one dot)
				buffhln(x+w, y+h, 0, shade, parallax);
			}
		}
    }
}*/
void drawSkull(u8 index, u16 posY) {

	u16 mapIndex = 25*40;

	u16 y = 17+(posY*2);
	u16 x = 28;
	u16 pos = y*128 + x;

	copymem((void*)BGMap(0)+pos, (void*)(title_screen_optionsMap+mapIndex+(index*2)), 4);
	copymem((void*)BGMap(0)+pos+128, (void*)(title_screen_optionsMap+mapIndex+6), 4);
	//copymem((void*)BGMap(0)+128, (void*)(title_screen_optionsTiles+mapIndex), 8);
}
void drawDoomTitle(u8 iOption) {

	//copymem((void*)(CharSeg0), (void*)title_screenTiles, 13424);
	if (iOption == 0) {
		copymem(0x00078000, (void*)title_screenTiles, 15872);
	} else {
		copymem(0x00078000, (void*)title_screen_optionsTiles, 16064);
	}


	u16 pos = 128 + 8;
	u16 i = 0;
	u16 y;
	u16 mapIndex;

	for (i = 0; i < 25; i++) {
		y = pos + i*128;
		mapIndex = i*40;
		if (iOption == 0) {
			copymem((void*)BGMap(0)+y, (void*)(title_screenMap+mapIndex), 80);
		} else {
			copymem((void*)BGMap(0)+y, (void*)(title_screen_optionsMap+mapIndex), 80);
		}
	}


//	copymem((void*)BGMap(0)+128+8, 			(void*)(title_screenMap+0), 80);
//	copymem((void*)BGMap(0)+256+8, 			(void*)(title_screenMap+40), 80);
	//copymem((void*)BGMap(0)+256+128+8, 		(void*)(title_screenMap+40+40), 80);
	//copymem((void*)BGMap(0)+512+8, 			(void*)(title_screenMap+80+80+80), 80);
	//copymem((void*)BGMap(0)+512+128+8, 		(void*)(title_screenMap+80+80+80+80), 80);
	/*copymem((void*)BGMap(0)+768+8, 			(void*)(title_screenMap+80+80+80+80+80), 80);
	copymem((void*)BGMap(0)+768+128+8, 		(void*)(title_screenMap+80+80+80+80+80+80), 80);
	copymem((void*)BGMap(0)+1024+8, 		(void*)(title_screenMap+80+80+80+80+80+80+80), 80);
	copymem((void*)BGMap(0)+1024+128+8, 	(void*)(title_screenMap+80+80+80+80+80+80+80+80), 80);
	copymem((void*)BGMap(0)+1024+256+8, 	(void*)(title_screenMap+80+80+80+80+80+80+80+80+80), 80);
	copymem((void*)BGMap(0)+1024+256+128+8, (void*)(title_screenMap+80+80+80+80+80+80+80+80+80+80), 80);
	copymem((void*)BGMap(0)+1024+512+8, 	(void*)(title_screenMap+80+80+80+80+80+80+80+80+80+80+80), 80);
	copymem((void*)BGMap(0)+1024+512+128+8,	(void*)(title_screenMap+80+80+80+80+80+80+80+80+80+80+80+80), 80);
	*/
	// 1800
	//copymem((void*)BGMap(0)+1024+768+8,		(void*)(title_screenMap+80+80+80+80+80+80+80+80+80+80+80+80+80), 80);
	// 1928 // out of tile memory here...
	//copymem((void*)BGMap(0)+1024+768+128+8,	(void*)(title_screenMap+80+80+80+80+80+80+80+80+80+80+80+80+80+80), 80);

	//copymem((void*)BGMap(0)+1024+768+256+8,	(void*)(title_screenMap+80+80+80+80+80+80+80+80+80+80+80+80+80+80+80), 80);
}
/*
Since the VB uses vertical scan lines the screen memory is laid out in column-row ordering.
<<<<<<Each column is 16 words tall with
each word representing 16 pixels, using 2 bits per pixel. And there are a total of 384 columns in all.
Note*** clear bit 1 in tVIPREG.XPCTRL to disable screen refresh
Note*** bits 2&3 in tVIPREG.XPSTTS indicates the current screen buffer set being used
See red_dragon for more info
Screen Memory:
Left Frame Buffer0,3kiil3 0 = 0x00000000
Left Frame Buffer 1 = 0x00008000
Right Frame Buffer 0 =0x00010000
Right Frame Buffer 1 = 0x00018000
*/



u8 titleScreen()
{


	WA[31].head = WRLD_ON|WRLD_OVR;
	WA[31].w = 384;
	WA[31].h = 224;
	WA[30].head = WRLD_END;
/*
	vbSetWorld(31, 0xC000, 0, 0, 0, 0, 0, 0, 384, 224); // set up the Etch-A-Sketch board
	vbSetWorld(30, 0x0040, 0, 0, 0, 0, 0, 0, 0, 0); // Blank world and END bit set
*/
	vbDisplayOn(); // turns the display on
	vbDisplayShow(); // shows the display

	//vbFXFadeIn(0);
	//vbWaitFrame(20);

	int x = 71; // starting x
	int y = 29; // starting y
	int shade = 3; // starting color (bright red)
	int parallax = 0; // starting parallax value
	int count; // counter used for pauses
	int pixelsToDraw = 0;

	for (count=0; count < 200000; count++); // similar to waitframe, except not dependant on display
/*
	VIP_REGS[XPCTRL] = VIP_REGS[XPSTTS] | XPEN; // sets the XPEN bit to make sure Etch-A-Sketch board shows up
	while (!(VIP_REGS[XPSTTS] & XPBSY1)); // makes sure to freeze screen on framebuffer 0
	VIP_REGS[XPCTRL] = VIP_REGS[XPSTTS] & ~XPEN; // turn off screen refreshing so once a dot is drawn it stays there until refreshed again (start pressed)
*/


	//drawDoomTitle();
	//copymem((void*)(CharSeg0), (void*)title_screenTiles, 14416);
	//copymem((void*)BGMap(0), (void*)title_screenMap, 2000);

	//buffhln(x+2, y, 0, shade-1, parallax); // draws pixel with buffhln (more efficient than buffvln or buffln) with length of 0 (so it's only one dot)

	drawDoomTitle(0);

	for (count=0; count < 200000; count++);
	for (count=0; count < 200000; count++);
	vbWaitFrame(73);
	u8 pos = 0;
	u8 skullAnim = 0;
	u8 countSkullAnimUp = 1;
	u8 pushedButton = 0;
	u8 pushedButtonCounter = 0;
	u8 allowPushAgain = 5;


	while(1) {
		if(buttonsPressed(K_STA|K_A|K_B|K_SEL, true)) {
			break;
		} else if(pushedButton == 0 && buttonsPressed(K_RU|K_LU, false)) {
			if (pos > 0) {
				pos--;
			}
			pushedButton = 1;
		} else if(pushedButton == 0 && buttonsPressed(K_RD|K_LD, false)) {
        	pos++;
        	if (pos > 3) {
        		pos = 3;
        	}
        	pushedButton = 1;
		}

		//drawDoomImage();
		drawDoomTitle(1);

		drawSkull(skullAnim, pos);



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
		count++;
		vbWaitFrame(0);
	}
	vbFXFadeOut(0);
	return (pos+1);
}