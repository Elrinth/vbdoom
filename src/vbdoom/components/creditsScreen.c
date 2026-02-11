#include <constants.h>
#include <controller.h>
#include <world.h>
#include <input.h>
#include <controller.h>
#include "../functions/sndplay.h"
#include <text.h>
#include <languages.h>
#include "creditsScreen.h"
#include <virtualboy_pixeldraw.h>

/* Font is already loaded at CharSeg3+0x1000 (chars 1792-2047) by initSystem().
 * printString() maps ASCII to those char indices via (char + 0x700). */
extern BYTE FontTiles[];

/* Planet Virtual Boy logo (208x32, 64 tiles + 26x4 map) */
extern unsigned int planet_virtualboyTiles[];
extern unsigned short planet_virtualboyMap[];

/* Logo tiles loaded at CharSeg3+0 = char index 1536 (0x600).
 * Map entries from grit are 0-based, so we add 0x600 to each. */
#define LOGO_CHAR_BASE  0x600
#define LOGO_TILES      64
#define LOGO_MAP_W      26
#define LOGO_MAP_H      4

/* Helper: draw a centered string on BGMap(0) at tile row y.
 * Screen is 48 tiles wide (384/8), so center = (48 - strlen) / 2. */
static void printCentered(u8 row, char *str) {
	u8 len = 0;
	char *p = str;
	while (*p++) len++;
	printString(0, (48 - len) / 2, row, str);
}

/* Draw the Planet VB logo onto BGMap(0) at tile row y, centered.
 * Logo is 26x4 tiles (208x32 px); screen is 48 tiles wide. */
static void drawLogo(u8 row) {
	u8 col = (48 - LOGO_MAP_W) / 2;  /* center horizontally */
	u8 r, c;
	for (r = 0; r < LOGO_MAP_H; r++) {
		for (c = 0; c < LOGO_MAP_W; c++) {
			u16 entry = planet_virtualboyMap[r * LOGO_MAP_W + c];
			/* Add char base offset to tile index (bits 0-10) */
			u16 tileIdx = entry & 0x07FF;
			u16 flags   = entry & 0xF800;
			BGMM[((row + r) << 6) + col + c] = flags | (tileIdx + LOGO_CHAR_BASE);
		}
	}
}

/* Last content row (tile row) -- scroll stops when this is near bottom */
#define CREDITS_LAST_ROW  58
/* Scroll speed: advance 1 pixel every SCROLL_EVERY frames */
#define SCROLL_EVERY  2
/* Delay before scrolling starts (in frames, ~50fps) */
#define SCROLL_DELAY  50

u8 creditsScreen()
{
	u16 frame = 0;
	u16 scrollY = 0;
	u16 maxScroll = (CREDITS_LAST_ROW - 27) * 8;  /* pixels */

	setmem((void*)BGMap(0), 0, 8192);
	WA[31].head = WRLD_ON|WRLD_OVR;
	WA[31].gx = 0;
	WA[31].gp = 0;
	WA[31].gy = 0;
	WA[31].mx = 0;
	WA[31].mp = 0;
	WA[31].my = 0;
	WA[31].w = 384;
	WA[31].h = 224;
	WA[30].head = WRLD_END;

	VIP_REGS[GPLT0] = 0xE4;
	VIP_REGS[GPLT1] = 0x00;
	VIP_REGS[GPLT2] = 0x84;
	VIP_REGS[GPLT3] = 0xE4;

	/* Load logo tiles at CharSeg3+0 (char 1536+) */
	copymem((void*)0x00078000, (void*)planet_virtualboyTiles, LOGO_TILES * 16);

	vbDisplayOn();

	/* === VB DOOM credits (visible immediately) === */
	printCentered( 1, "- CREDITS -");

	printCentered( 3, "VB DOOM");
	printCentered( 4, "PROGRAMMING");
	printCentered( 5, "VIRTUALBOY");

	printCentered( 7, "BASED ON");
	printCentered( 8, "WOLFENSTEIN 3D & DOOM");
	printCentered( 9, "BY ID SOFTWARE");

	printCentered(11, "GAME DESIGN");
	printCentered(12, "JOHN ROMERO");

	printCentered(14, "PROGRAMMING");
	printCentered(15, "JOHN CARMACK");

	printCentered(17, "ART");
	printCentered(18, "ADRIAN CARMACK");
	printCentered(19, "KEVIN CLOUD");

	printCentered(21, "MUSIC");
	printCentered(22, "BOBBY PRINCE");

	printCentered(24, "SOUND EFFECTS");
	printCentered(25, "ID SOFTWARE");

	printCentered(27, "MADE FOR VIRTUAL BOY");

	/* === Planet Virtual Boy (scrolls into view) === */
	drawLogo(29);  /* 4 tiles tall, rows 29-32 */

	printCentered(34, "GUYPERFECT");
	printCentered(35, "RETROONYX");
	printCentered(36, "KR1SSE");
	printCentered(37, "BLITTER");
	printCentered(38, "THE BLENDER FIDDLE");
	printCentered(39, "THUNDERSTRUCK");
	printCentered(40, "ENTHUSI");
	printCentered(41, "JORGECHE");
	printCentered(42, "UNTITLED-1");
	printCentered(43, "SPEEDYINC");
	printCentered(44, "LAYERCAKE");
	printCentered(45, "MAUSIMUS");
	printCentered(46, "MORITARI");
	printCentered(47, "RAYCEARONI");
	printCentered(48, "VIRTUALCHRIS(T)");
	printCentered(49, "MUMPHY");
	printCentered(50, "DASI");
	printCentered(51, "FWOW13");
	printCentered(52, "THEREDMENACE");
	printCentered(53, "DREAMBEAN");
	printCentered(54, "SUPER BROS.");
	printCentered(55, "SUCROSE");
	printCentered(56, "JNL");
	printCentered(57, "PANTHERS426");

	vbDisplayShow();

	while(1) {
		if(buttonsPressed(K_STA|K_A|K_B|K_SEL, true)) {
			break;
		}

		/* Scroll after initial delay */
		if (frame >= SCROLL_DELAY && scrollY < maxScroll) {
			if ((frame % SCROLL_EVERY) == 0) {
				scrollY++;
				WA[31].my = scrollY;
			}
		}

		updateMusic(true);
		frame++;
		vbWaitFrame(0);
	}
	setmem((void*)BGMap(0), 0, 8192);
	return 0;
}