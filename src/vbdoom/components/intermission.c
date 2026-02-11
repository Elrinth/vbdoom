/*
 * intermission.c -- Level intermission screen (le_map.png) + stats screen
 *
 * Shows the episode map between levels with a "bleed-down" wipe reveal,
 * skull with blinking eyes animation, and bleed-down hide.
 * Then shows the level completion stats (kills, items, secrets, time).
 *
 * VRAM note: this function overwrites 0x78000+ with le_map/stats tiles.
 * The caller MUST restore all game VRAM (HUD, weapons, faces, walls, etc.)
 * after this function returns.
 */

#include <constants.h>
#include <controller.h>
#include <world.h>
#include <input.h>
#include <text.h>
#include "../assets/images/win_gfx.h"
#include "../functions/sndplay.h"
#include "../assets/audio/doom_sfx.h"

extern BYTE le_mapTiles[];
extern unsigned short le_mapMap[];

/* Big font from the HUD tileset (vb_doom.c) */
extern const unsigned int vb_doomTiles[];
extern const unsigned short vb_doomMap[];

/* Level tracking globals from gameLoop.c / enemy.c / pickup.c */
extern u16 g_levelFrames;
extern u8 g_enemiesKilled;
extern u8 g_totalEnemies;
extern u8 g_itemsCollected;
extern u8 g_totalItems;
extern u8 g_secretsFound;
extern u8 g_totalSecrets;
extern u8 currentLevel;

/* le_map is 40 tiles wide x 26 tiles tall (320 x 208 pixels) -- same as title screen */
#define LEMAP_W    40
#define LEMAP_H    26
#define LEMAP_PX_W 320
#define LEMAP_PX_H 208

/* Skull data sits in the last tile row (row 25) of the map.
 * Layout matches the title screen convention:
 *   entries 0-2:  skull body top row    -> BGMap(1) row 0
 *   entries 3-5:  skull body bottom row -> BGMap(1) row 1
 *   entries 6-8:  skull black top row   -> BGMap(2) row 0
 *   entries 9-11: skull black bottom row-> BGMap(2) row 1
 *   entries 12-14: eye animation frames -> BGMap(3)
 */
#define SKULL_MAP_INDEX  (25 * LEMAP_W)

/* Centering the 320-px image on the 384-px screen (same as title screen) */
#define LEMAP_GX  ((384 - LEMAP_PX_W) >> 1)  /* = 32 */
#define LEMAP_GY  12

/* Per-level skull positions on the episode map (x,y offsets from LEMAP_GX/GY).
 * These mark the player's current level on the map image.
 * E1M1 = level 1, E1M2 = level 2, E1M3 = level 3. */
static const s16 SKULL_POS_X[] = { 0, 148, 230, 130 };  /* [0]=unused, [1]=E1M1, [2]=E1M2, [3]=E1M3 */
static const s16 SKULL_POS_Y[] = { 0, 182, 150, 110 };  /* Approximate positions on le_map */

/* Bleed-down speed: pixels revealed per frame */
#define BLEED_STEP  8

/* How long to display (in frames at ~20fps) before auto-dismiss */
#define DISPLAY_FRAMES  200  /* ~10 seconds */


static void drawIntermissionSkull(u8 eyeFrame) {
	/* Skull body -> BGMap(1): 3 tiles wide x 2 rows */
	copymem((void*)BGMap(1),     (void*)(le_mapMap + SKULL_MAP_INDEX),     6);
	copymem((void*)BGMap(1)+128, (void*)(le_mapMap + SKULL_MAP_INDEX + 3), 6);

	/* Skull black layer -> BGMap(2): 3 tiles wide x 2 rows */
	copymem((void*)BGMap(2),     (void*)(le_mapMap + SKULL_MAP_INDEX + 6), 6);
	copymem((void*)BGMap(2)+128, (void*)(le_mapMap + SKULL_MAP_INDEX + 9), 6);

	/* Set palette 2 for the black layer (same as title screen) */
	{
		u8 xx, yy;
		for (yy = 0; yy < 4; yy++) {
			for (xx = 0; xx < 4; xx++) {
				BGM_PALSET(2, xx, yy, BGM_PAL2);
			}
		}
	}

	/* Eye frame -> BGMap(3): 1 tile (selected by eyeFrame 0/1/2) */
	copymem((void*)BGMap(3), (void*)(le_mapMap + SKULL_MAP_INDEX + 12 + eyeFrame), 2);
}


void showIntermission(void) {
	u16 i;

	/* ---- Clear BGMaps 0-3 ---- */
	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192);

	/* ---- Configure worlds ---- */
	/* World 31: main le_map image */
	vbSetWorld(31, WRLD_ON|0, 0, 0, 0, 0, 0, 0, LEMAP_PX_W, 0);
	/* World 30: skull body (BGMap 1) */
	vbSetWorld(30, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 32, 32);
	/* World 29: skull black layer (BGMap 2) */
	vbSetWorld(29, WRLD_ON|2, 0, 0, 0, 0, 0, 0, 32, 32);
	/* World 28: skull eyes (BGMap 3) */
	vbSetWorld(28, WRLD_ON|3, 0, 0, 0, 0, 0, 0, 8, 8);
	/* Terminate world list */
	WA[27].head = WRLD_END;

	/* Position main image (centered) */
	WA[31].gx = LEMAP_GX;
	WA[31].gy = LEMAP_GY;

	/* Position skull worlds based on current level */
	{
		u8 lvl = currentLevel;
		s16 skullX, skullY;
		if (lvl < 1 || lvl > 3) lvl = 1;
		skullX = LEMAP_GX + SKULL_POS_X[lvl];
		skullY = LEMAP_GY + SKULL_POS_Y[lvl];
		WA[30].gx = WA[29].gx = skullX;
		WA[30].gy = WA[29].gy = skullY;
		WA[28].gx = skullX + 5;
		WA[28].gy = skullY + 4;
	}

	/* Skull and eye worlds start hidden during bleed-in */
	WA[30].head = 0;
	WA[29].head = 0;
	WA[28].head = 0;

	/* ---- Set palettes ---- */
	VIP_REGS[GPLT0] = 0xE4;  /* Normal: 11 10 01 00 */
	VIP_REGS[GPLT1] = 0x00;
	VIP_REGS[GPLT2] = 0x84;  /* Black layer palette */
	VIP_REGS[GPLT3] = 0xE4;

	/* ---- Load tile data to VRAM ---- */
	copymem((void*)0x00078000, (void*)le_mapTiles, 16160);  /* 1010 tiles * 16 bytes */

	/* ---- Fill BGMap(0) with le_map image (25 rows, skip skull row) ---- */
	for (i = 0; i < 25; i++) {
		copymem((void*)BGMap(0) + i * 128,
		        (void*)(le_mapMap + i * LEMAP_W),
		        LEMAP_W * 2);
	}

	/* ---- Draw skull into BGMaps 1-3 ---- */
	drawIntermissionSkull(0);

	/* Restore brightness after the caller's vbFXFadeOut().
	 * WA[31].h is still 0 so nothing is visible yet --
	 * the bleed-down wipe below controls the actual reveal. */
	vbFXFadeIn(0);

	/* ---- Bleed-down reveal (top to bottom) ---- */
	for (i = 0; i <= LEMAP_PX_H; i += BLEED_STEP) {
		WA[31].h = i;
		updateMusic(true);
		vbWaitFrame(0);
	}
	/* Ensure final height is exact */
	WA[31].h = LEMAP_PX_H;

	/* Enable skull worlds now that image is fully revealed */
	WA[30].head = WRLD_ON | 1;
	WA[29].head = WRLD_ON | 2;
	WA[28].head = WRLD_ON | 3;
	vbWaitFrame(0);

	/* ---- Skull animation + wait loop ---- */
	{
		u8 skullAnim = 0;
		u8 countUp = 1;
		u8 animTimer = 0;
		u16 frameCount = 0;
		u16 keyInputs;

		while (frameCount < DISPLAY_FRAMES) {
			keyInputs = vbReadPad();
			if (keyInputs & (K_STA | K_A | K_B)) {
				break;  /* button press dismisses early */
			}

			/* Eye blink animation (cycle: 0->1->2->1->0->...) */
			animTimer++;
			if (animTimer > 6) {
				animTimer = 0;
				if (countUp) {
					skullAnim++;
					if (skullAnim > 2) {
						countUp = 0;
						skullAnim = 1;
					}
				} else {
					if (skullAnim == 0) {
						countUp = 1;
						skullAnim++;
					} else {
						skullAnim--;
					}
				}
			}

			drawIntermissionSkull(skullAnim);
			updateMusic(true);
			vbWaitFrame(0);
			frameCount++;
		}
	}

	/* ---- Bleed-down hide (shrink from top) ---- */
	/* Hide skull first */
	WA[30].head = 0;
	WA[29].head = 0;
	WA[28].head = 0;

	for (i = LEMAP_PX_H; i > 0; i -= BLEED_STEP) {
		WA[31].gy = LEMAP_GY + (LEMAP_PX_H - i);
		WA[31].my = (LEMAP_PX_H - i);
		WA[31].h = i;
		updateMusic(true);
		vbWaitFrame(0);
	}
	WA[31].h = 0;
	vbWaitFrame(0);

	/* ---- Fade to black for clean transition ---- */
	vbFXFadeOut(0);
	vbWaitFrame(10);

	/* ---- Cleanup: clear BGMaps ---- */
	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192);
}


/* ========================================================================
 * Level completion stats screen
 * ========================================================================
 * Displays: E1M1 FINISHED
 *           TIME   00:23
 *           KILLS  100%
 *           ITEMS   50%
 *           SECRET 100%
 *
 * Uses BGMap(0) on World 31.  All tile data is loaded to 0x78000+.
 * Big font digits come from vb_doomTiles/vb_doomMap.
 * Label graphics come from win_gfx.c.
 */

/* VRAM layout for stats screen (all at CharSeg3 = 0x78000):
 * 0 - 128  : big font digits (from vb_doomTiles) -- reuse existing HUD tiles
 * 129+     : win_gfx label tiles (loaded consecutively)
 */
#define STATS_LABEL_CHAR_BASE  129

/* BGMap row stride in bytes */
#define BGMAP_ROW_BYTES  128  /* 64 entries * 2 bytes (but only 48 visible) */

/* vb_doomMap layout: row 16 = big digit top halves, row 17 = bottom halves.
 * Each digit is 2 entries wide. Digit N starts at column N*2.
 * % is at columns 20-21. : is at column 22. */
#define BIGFONT_ROW_TOP    (16 * 48)
#define BIGFONT_ROW_BOT    (17 * 48)


/* Draw big font 2-tile-wide digit at (bgmapCol, bgmapRow) in BGMap(0).
 * digit: 0-9 for numbers. */
static void statsDrawDigit(u8 bgmapCol, u8 bgmapRow, u8 digit) {
	u16 mapOfs = bgmapRow * 64 + bgmapCol;  /* entry offset in BGMap */
	u16 srcTop = BIGFONT_ROW_TOP + digit * 2;
	u16 srcBot = BIGFONT_ROW_BOT + digit * 2;
	/* Top row: 2 entries */
	copymem((void*)BGMap(0) + mapOfs * 2,
	        (void*)(vb_doomMap + srcTop), 4);
	/* Bottom row: 2 entries (next BGMap row = +64 entries) */
	copymem((void*)BGMap(0) + (mapOfs + 64) * 2,
	        (void*)(vb_doomMap + srcBot), 4);
}

/* Draw % symbol (2 tiles wide) */
static void statsDrawPercent(u8 bgmapCol, u8 bgmapRow) {
	u16 mapOfs = bgmapRow * 64 + bgmapCol;
	u16 srcTop = BIGFONT_ROW_TOP + 20;  /* % at col 20 */
	u16 srcBot = BIGFONT_ROW_BOT + 20;
	copymem((void*)BGMap(0) + mapOfs * 2,
	        (void*)(vb_doomMap + srcTop), 4);
	copymem((void*)BGMap(0) + (mapOfs + 64) * 2,
	        (void*)(vb_doomMap + srcBot), 4);
}

/* Draw : symbol (1 tile wide) */
static void statsDrawColon(u8 bgmapCol, u8 bgmapRow) {
	u16 mapOfs = bgmapRow * 64 + bgmapCol;
	u16 srcTop = BIGFONT_ROW_TOP + 22;  /* : at col 22 */
	u16 srcBot = BIGFONT_ROW_BOT + 22;
	copymem((void*)BGMap(0) + mapOfs * 2,
	        (void*)(vb_doomMap + srcTop), 2);
	copymem((void*)BGMap(0) + (mapOfs + 64) * 2,
	        (void*)(vb_doomMap + srcBot), 2);
}

/* Draw a label image at (bgmapCol, bgmapRow) using pre-loaded tiles.
 * labelMap = array of u16 map entries (with char indices 0..N).
 * charBase = VRAM char index where label tiles were loaded.
 * cols, rows = label dimensions in tiles. */
static void statsDrawLabel(u8 bgmapCol, u8 bgmapRow,
                           const unsigned short *labelMap, u16 charBase,
                           u8 cols, u8 rows) {
	u8 r, c;
	for (r = 0; r < rows; r++) {
		for (c = 0; c < cols; c++) {
			u16 entry = labelMap[r * cols + c];
			u16 charIdx = entry & 0x07FF;
			u16 flags = entry & 0xF800;
			u16 newEntry;
			if (charIdx == 0) {
				newEntry = 0;
			} else {
				newEntry = flags | ((charIdx + charBase) & 0x07FF);
			}
			{
				u16 mapOfs = (bgmapRow + r) * 64 + bgmapCol + c;
				*((u16*)((u8*)BGMap(0) + mapOfs * 2)) = newEntry;
			}
		}
	}
}

/* Draw a number with optional leading zeros as big font (for percentage display).
 * val = 0-100, startCol = BGMap column to start at.
 * Returns next column after the drawn digits. */
static u8 statsDrawNumber(u8 bgmapCol, u8 bgmapRow, u8 val) {
	u8 hundreds = val / 100;
	u8 tens = (val / 10) % 10;
	u8 ones = val % 10;
	u8 col = bgmapCol;

	if (hundreds > 0) {
		statsDrawDigit(col, bgmapRow, hundreds);
		col += 2;
	}
	if (hundreds > 0 || tens > 0) {
		statsDrawDigit(col, bgmapRow, tens);
		col += 2;
	}
	statsDrawDigit(col, bgmapRow, ones);
	col += 2;
	return col;
}

/* Draw time as MM:SS */
static void statsDrawTime(u8 bgmapCol, u8 bgmapRow, u16 totalSeconds) {
	u8 mm = (u8)(totalSeconds / 60);
	u8 ss = (u8)(totalSeconds % 60);
	u8 col = bgmapCol;

	/* Minutes: always 2 digits */
	statsDrawDigit(col, bgmapRow, mm / 10);
	col += 2;
	statsDrawDigit(col, bgmapRow, mm % 10);
	col += 2;

	/* Colon */
	statsDrawColon(col, bgmapRow);
	col += 1;

	/* Seconds: always 2 digits */
	statsDrawDigit(col, bgmapRow, ss / 10);
	col += 2;
	statsDrawDigit(col, bgmapRow, ss % 10);
}


void showLevelStats(void) {
	u16 charNext;
	u16 witimeCharBase, wiostkCharBase, wiostiCharBase, wiscrtCharBase;
	u16 wienterCharBase;
	u16 totalSeconds;
	u8 killPct, itemPct, secretPct;
	u8 dispKill, dispItem, dispSecret;
	u16 keyInputs;

	/* ---- Compute stats ---- */
	totalSeconds = g_levelFrames / 20;  /* 20 fps */
	killPct   = (g_totalEnemies > 0)  ? (u8)((u16)g_enemiesKilled * 100 / g_totalEnemies) : 100;
	itemPct   = (g_totalItems > 0)    ? (u8)((u16)g_itemsCollected * 100 / g_totalItems) : 100;
	secretPct = (g_totalSecrets > 0)  ? (u8)((u16)g_secretsFound * 100 / g_totalSecrets) : 100;

	/* ---- Clear BGMap(0) ---- */
	setmem((void*)BGMap(0), 0, 8192);

	/* ---- Load tile data ---- */
	/* 1. Load HUD big font tiles (129 tiles, includes digits, %, :) */
	copymem((void*)0x00078000, (void*)vb_doomTiles, 2064);

	/* 2. Load win_gfx label tiles after HUD tiles */
	charNext = STATS_LABEL_CHAR_BASE;

	wienterCharBase = charNext;
	copymem((void*)(0x00078000 + charNext * 16), (void*)wienterTiles, WIENTER_TILE_BYTES);
	charNext += WIENTER_TILES;

	witimeCharBase = charNext;
	copymem((void*)(0x00078000 + charNext * 16), (void*)witimeTiles, WITIME_TILE_BYTES);
	charNext += WITIME_TILES;

	wiostkCharBase = charNext;
	copymem((void*)(0x00078000 + charNext * 16), (void*)wiostkTiles, WIOSTK_TILE_BYTES);
	charNext += WIOSTK_TILES;

	wiostiCharBase = charNext;
	copymem((void*)(0x00078000 + charNext * 16), (void*)wiostiTiles, WIOSTI_TILE_BYTES);
	charNext += WIOSTI_TILES;

	wiscrtCharBase = charNext;
	copymem((void*)(0x00078000 + charNext * 16), (void*)wiscrtTiles, WISCRT_TILE_BYTES);
	charNext += WISCRT_TILES;

	/* ---- Configure worlds ---- */
	/* World 31: stats screen (full-width) */
	vbSetWorld(31, WRLD_ON|0, 0, 0, 0, 0, 0, 0, 384, 224);
	WA[31].gx = 0;
	WA[31].gy = 0;
	WA[30].head = WRLD_END;

	/* ---- Set palettes ---- */
	VIP_REGS[GPLT0] = 0xE4;
	VIP_REGS[GPLT1] = 0x00;
	VIP_REGS[GPLT2] = 0x84;
	VIP_REGS[GPLT3] = 0xE4;

	/* ---- Draw the static layout ---- */
	/* Row 0 (y=0):  "E1M1" using small font / big digits
	 * We'll draw level name using big font digits: E 1 M 1 */
	{
		/* "E1M" prefix -- use the small font row (row 18) if available,
		 * or just draw the level number with big font.
		 * For simplicity: "E1M1 FINISHED" label graphic */
		u8 levelDigit = currentLevel;
		u8 startCol = 7;  /* center roughly on 384px / 8 = 48 tile columns */

		/* Draw "FINISHED" label */
		statsDrawLabel(startCol, 2, wienterMap, wienterCharBase,
		               WIENTER_COLS, WIENTER_ROWS);

		/* Draw level number "1" or "2" above FINISHED, centered */
		statsDrawDigit(startCol + 4, 0, levelDigit);
	}

	/* ---- Fade in ---- */
	vbFXFadeIn(0);

	/* ---- Counting-up animation (Doom style) ---- */
	dispKill = 0;
	dispItem = 0;
	dispSecret = 0;

	/* Draw static labels first */
	/* Row 5-6: TIME label + value */
	statsDrawLabel(6, 5, witimeMap, witimeCharBase, WITIME_COLS, WITIME_ROWS);
	statsDrawTime(20, 5, totalSeconds);

	/* Row 8-9: KILLS */
	statsDrawLabel(6, 8, wiostkMap, wiostkCharBase, WIOSTK_COLS, WIOSTK_ROWS);

	/* Row 11-12: ITEMS */
	statsDrawLabel(6, 11, wiostiMap, wiostiCharBase, WIOSTI_COLS, WIOSTI_ROWS);

	/* Row 14-15: SECRET */
	statsDrawLabel(6, 14, wiscrtMap, wiscrtCharBase, WISCRT_COLS, WISCRT_ROWS);

	vbWaitFrame(0);

	/* Count up kills */
	while (dispKill < killPct) {
		dispKill += 2;
		if (dispKill > killPct) dispKill = killPct;
		{
			u8 nextCol = statsDrawNumber(20, 8, dispKill);
			statsDrawPercent(nextCol, 8);
		}
		playPlayerSFX(SFX_PISTOL);
		updateMusic(true);
		vbWaitFrame(0);
		updateMusic(true);
		vbWaitFrame(0);
		keyInputs = vbReadPad();
		if (keyInputs & (K_STA | K_A)) {
			dispKill = killPct;
			dispItem = itemPct;
			dispSecret = secretPct;
			playPlayerSFX(SFX_BARREL_EXPLODE);
			goto draw_final;
		}
	}
	/* Draw final kill value + completion sound + pause */
	{
		u8 nextCol = statsDrawNumber(20, 8, killPct);
		statsDrawPercent(nextCol, 8);
	}
	playPlayerSFX(SFX_BARREL_EXPLODE);
	{ u8 w; for (w = 0; w < 20; w++) { updateMusic(true); vbWaitFrame(0); } }

	/* Count up items */
	while (dispItem < itemPct) {
		dispItem += 2;
		if (dispItem > itemPct) dispItem = itemPct;
		{
			u8 nextCol = statsDrawNumber(20, 11, dispItem);
			statsDrawPercent(nextCol, 11);
		}
		playPlayerSFX(SFX_PISTOL);
		updateMusic(true);
		vbWaitFrame(0);
		updateMusic(true);
		vbWaitFrame(0);
		keyInputs = vbReadPad();
		if (keyInputs & (K_STA | K_A)) {
			dispItem = itemPct;
			dispSecret = secretPct;
			playPlayerSFX(SFX_BARREL_EXPLODE);
			goto draw_final;
		}
	}
	{
		u8 nextCol = statsDrawNumber(20, 11, itemPct);
		statsDrawPercent(nextCol, 11);
	}
	playPlayerSFX(SFX_BARREL_EXPLODE);
	{ u8 w; for (w = 0; w < 20; w++) { updateMusic(true); vbWaitFrame(0); } }

	/* Count up secrets */
	while (dispSecret < secretPct) {
		dispSecret += 2;
		if (dispSecret > secretPct) dispSecret = secretPct;
		{
			u8 nextCol = statsDrawNumber(20, 14, dispSecret);
			statsDrawPercent(nextCol, 14);
		}
		playPlayerSFX(SFX_PISTOL);
		updateMusic(true);
		vbWaitFrame(0);
		updateMusic(true);
		vbWaitFrame(0);
		keyInputs = vbReadPad();
		if (keyInputs & (K_STA | K_A)) {
			dispSecret = secretPct;
			playPlayerSFX(SFX_BARREL_EXPLODE);
			goto draw_final;
		}
	}

draw_final:
	/* Draw all final values */
	{
		u8 nextCol;
		nextCol = statsDrawNumber(20, 8, killPct);
		statsDrawPercent(nextCol, 8);
		nextCol = statsDrawNumber(20, 11, itemPct);
		statsDrawPercent(nextCol, 11);
		nextCol = statsDrawNumber(20, 14, secretPct);
		statsDrawPercent(nextCol, 14);
	}
	playPlayerSFX(SFX_BARREL_EXPLODE);
	vbWaitFrame(0);

	/* ---- Wait for button press to continue ---- */
	{
		u16 waitTimer = 0;
		while (waitTimer < 400) {  /* max ~20 seconds */
			keyInputs = vbReadPad();
			if (keyInputs & (K_STA | K_A | K_B)) break;
			updateMusic(true);
			vbWaitFrame(0);
			waitTimer++;
		}
	}

	/* ---- Fade out ---- */
	vbFXFadeOut(0);
	vbWaitFrame(10);

	/* ---- Cleanup ---- */
	setmem((void*)BGMap(0), 0, 8192);
}
