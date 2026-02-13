#include <constants.h>
#include <controller.h>
#include <world.h>
#include <input.h>
#include <controller.h>
#include <text.h>
#include <languages.h>
#include "menu_options.h"
#include <virtualboy_pixeldraw.h>
#include "rumble.h"
#include "sfx.h"
#include "dph9.h"
#include "../functions/sndplay.h"
#include "../functions/timer.h"
#include "../assets/audio/doom_sfx.h"

/* Rumble intensity table (mirrors gameLoop.c): maps setting (1-9) to {freq, effect, dmgDur, secDur}. */
typedef struct { u8 freq; u8 effect; u8 dmgDur; u8 secDur; } RumbleLevel;
static const RumbleLevel g_rumbleLevels[9] = {
    {RUMBLE_FREQ_160HZ, 0x01,  3, 1},
    {RUMBLE_FREQ_160HZ, 0x0E,  4, 1},
    {RUMBLE_FREQ_160HZ, 0x1B,  5, 2},
    {RUMBLE_FREQ_240HZ, 0x28,  6, 2},
    {RUMBLE_FREQ_240HZ, 0x35,  7, 2},
    {RUMBLE_FREQ_320HZ, 0x42,  8, 3},
    {RUMBLE_FREQ_320HZ, 0x4F,  8, 3},
    {RUMBLE_FREQ_400HZ, 0x5C, 10, 3},
    {RUMBLE_FREQ_400HZ, 0x7B, 12, 4},
};

extern BYTE title_screen_optionsTiles[];
extern unsigned short title_screen_optionsMap[];


void drawOptionsSkull(u8 index, u16 posY) {

	u16 mapIndex = 1000; // 25*40

	u16 y = 17+(posY*2);
	u16 x = 28;
	u16 pos = y*128 + x;

	WA[29].gy = WA[30].gy = WA[31].gy + 118+((posY==3?4:posY)*16);
	/* +32 so skull stays centered (world is now full 384px, content offset 32px) */
	WA[29].gx = WA[30].gx = WA[31].gx + 62 + 32;

	WA[28].gy = WA[29].gy+4;
	WA[28].gx = WA[30].gx+5;

	// draw skull in second layer
	copymem((void*)BGMap(1), (void*)(title_screen_optionsMap+mapIndex), 6);
	copymem((void*)BGMap(1)+128, (void*)(title_screen_optionsMap+mapIndex+3), 6);

	copymem((void*)BGMap(2), (void*)(title_screen_optionsMap+mapIndex+6), 6);
	copymem((void*)BGMap(2)+128, (void*)(title_screen_optionsMap+mapIndex+9), 6);
	u8 xx,yy;

	for (yy = 0; yy < 46; yy++) {
		for (xx = 0; xx < 40; xx++) {
			BGM_PALSET(2, xx, yy, BGM_PAL2);
		}
	}

	//copymem((void*)BGMap(0)+pos+256, (void*)(title_screen_optionsMap+mapIndex+4), 2); //eyes
	copymem((void*)BGMap(3), (void*)(title_screen_optionsMap+mapIndex+12+index), 2); //eyes
	//copymem((void*)BGMap(0)+128, (void*)(title_screen_optionsTiles+mapIndex), 8);
}
#define OPTIONS_MAP_LEFT_OFF  8  /* 4-tile offset so left border is 0 */

void hideNumbers() {
	u16 mapIndex = 1000; // 25*40
	u8 numberOfChars = 0;
	for (numberOfChars = 0; numberOfChars < 20; numberOfChars++) {
		copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*21 + 20 + (numberOfChars*2), (void*)(title_screen_optionsMap+mapIndex+15), 2);
		copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*22 + 20 + (numberOfChars*2), (void*)(title_screen_optionsMap+mapIndex+15), 2);
	}
}
void drawNumbers(Settings *settings) {
	u16 mapIndex = 840;

	// sfx
	copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*15 + 60, (void*)(title_screen_optionsMap+mapIndex+10+((*settings).sfx*2)), 4);
	copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*16 + 60, (void*)(title_screen_optionsMap+mapIndex+10+40+((*settings).sfx*2)), 4);

	// music
	copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*17 + 60, (void*)(title_screen_optionsMap+mapIndex+10+((*settings).music*2)), 4);
	copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*18 + 60, (void*)(title_screen_optionsMap+mapIndex+10+40+((*settings).music*2)), 4);

	// rumble
	copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*19 + 60, (void*)(title_screen_optionsMap+mapIndex+10+((*settings).rumble*2)), 4);
	copymem((void*)BGMap(0)+OPTIONS_MAP_LEFT_OFF+128*20 + 60, (void*)(title_screen_optionsMap+mapIndex+10+40+((*settings).rumble*2)), 4);
}

/* Clear BGMap(0) border so full screen shows 0 outside 320x200 content (real hardware). */
static void clearOptionsBorder(void) {
	u16 row;
	/* Left: cols 0-3 (8 bytes) for content rows 0-24 */
	for (row = 0; row < 25; row++) {
		setmem((void*)BGMap(0) + row * 128, 0, 8);
	}
	/* Right: cols 44-63 (40 bytes) for content rows 0-24 */
	for (row = 0; row < 25; row++) {
		setmem((void*)BGMap(0) + row * 128 + 88, 0, 40);
	}
	/* Bottom: full rows 25-31 */
	for (row = 25; row < 32; row++) {
		setmem((void*)BGMap(0) + row * 128, 0, 128);
	}
}

void drawOptionsScreen(u8 iOption) {
	u16 i;
	u16 y;
	u16 mapIndex;
	/* Draw at 4-tile (8 byte) offset so left 32px is 0; avoids upper-left garbage */
	for (i = 0; i < 25; i++) {
		y = 8 + i * 128;
		mapIndex = i * 40;
		copymem((void*)BGMap(0) + y, (void*)(title_screen_optionsMap + mapIndex), 80);
	}
	clearOptionsBorder();
}

const signed long ST[] = {
	-31,-29,-27,-25,-23,-21,-19,-17,
	-15,-13,-11, -9, -7, -5, -3, -1,
	  1,  3,  5,  7,  9, 11, 13, 15,
	 17, 19, 21, 23, 25, 27, 29, 31
/*
	  7,  6,  4,  1,  1,  4,  4,  2,
     	 -1,  3,  7,  8,  7,  3,  1,  8,
     	  6,-15,-31,  9, 11,  4,  0, -1,
     	 -1,  2,  6,  3,  1,  3,  6,  8*/
};

void initAudio() {
	copymem((void*)WAVEDATA1, (void*)ST, 128);
	copymem((void*)WAVEDATA2, (void*)ST, 128);
	copymem((void*)WAVEDATA3, (void*)ST, 128);
	copymem((void*)WAVEDATA4, (void*)ST, 128);
	copymem((void*)WAVEDATA5, (void*)ST, 128);
}
/*
typedef struct {
	u16* freq;
	u8* freq_l;
	u8 freq_c;

	u8* env0;
	u8* env0_l;
	u8 env0_c;

	u8* env1;
	u8* env1_l;
	u8 env1_c;

	bool isPlaying; // true / false

	u8 freq_i;
	u8 env0_i;
	u8 env1_i;

	u8 freqCounter; // 0
	u8 env0Counter; // 0
	u8 env1Counter; // 0
} SFX;
	*/

SFX sfx = {
	(u16[]){0x652,0x331,0x040,},
	(u8[]){1,2,2}, // 10 ticks
	3,
	// last bit: 0 = 0.96ms or 1 = 7.68 (CLK speed)
	// bit 4-7  = initial volume // bit interval 0, 1, 2 or 4. if 0 then no freq mod applied (
	// bit 3 = direction (0 = fade, 1 = grow)
	// bit 0->2 = interval
	(u8[]){0b11110000,0b11010000,0b10111000,}, // env0 fade
	(u8[]){1,2,1},
	3,

	// bit 0 = enabled or not
	// bit 1 = looping envelope
	// bit 4,5,6 = for sweep and only available on channel 5
	(u8[]){0b0000011,}, // env1
	(u8[]){1,},
	1,

	false,
	0,0,0, // indexes
	0,0,0, // counters

	0x00,
	false
};
SFX sfx2 = {
	(u16[]){0x252,0x531,0x140,},
	(u8[]){1,1,1}, // 10 ticks
	3,
	(u8[]){0b11110000,0b11010000,0b10111000,}, // env0 fade
	(u8[]){1,2,1},
	3,
	(u8[]){0b0000000,}, // env1
	(u8[]){1,},
	1,
	false,
	0,0,0,
	0,0,0,

	0x00,
	false
};
u8 isPlayingSfx = 0;


void playSFX(SFX *iSfx, u8 *audioLevel) {
	(*iSfx).isPlaying = true;
	// reset index counters etc.
	(*iSfx).freq_i = 0;
	(*iSfx).env0_i = 0;
    (*iSfx).env1_i = 0;
    (*iSfx).freqCounter = 0;
    (*iSfx).env0Counter = 0;
    (*iSfx).env1Counter = 0;
	SND_REGS[(*iSfx).channel].SxINT = 0b10000000;
	SND_REGS[(*iSfx).channel].SxLRV = ((*audioLevel) << 4) | (*audioLevel);

	SND_REGS[(*iSfx).channel].SxFQL = (*iSfx).freq[0] & 0xFF;
	SND_REGS[(*iSfx).channel].SxFQH = (*iSfx).freq[0] >> 8;
	SND_REGS[(*iSfx).channel].SxEV0 = (*iSfx).env0[0];
	SND_REGS[(*iSfx).channel].SxEV1 = (*iSfx).env1[0];

}


Music music = {
	(SFX[]){
		{
			(u16[]){E_3, PAU, E_3, PAU, E_4, E_3, PAU, E_3, PAU, D_4, E_3, PAU, E_3, PAU, C_4, E_3, PAU, E_3, PAU, AS3, E_3, PAU, E_3, PAU, B_3, C_4, E_3, PAU, E_3, PAU, E_4, E_3, PAU, E_3, PAU, D_4, E_3, PAU, E_3, PAU, C_4, E_3, PAU, E_3, PAU,  AS3 },
			(u8[]){   2,   2,   2,   2,   4,   2,   2,   2,   2,   4,   2,   2,   2,   2,   4,  2,    2,   2,   2,   4,   2,   2,   2,   2,   4,   4,   2,   2,   2,   2,   4,   2,   2,   2,   2,   4,   2,   2,   2,   2,   4,   2,   2,   2,   2,   20 },
			46,
			(u8[]){0b11110000,0b11010000,0b10111000,}, // env0 fade
			(u8[]){1,},
			3,
			(u8[]){0b0000000,}, // env1
			(u8[]){1,},
			1,
			false,
			0,0,0,
			0,0,0,

			0x01,
			true
		},
		{
			(u16[]){E_4, PAU, DS7, PAU, },
			(u8[]){   2, 6,  1,   7,   },
			4,
			(u8[]){0b11110000}, // env0 fade
			(u8[]){16,},
			1,
			(u8[]){0b0000000,}, // env1
			(u8[]){16,},
			1,
			false,
			0,0,0,
			0,0,0,

			0x05,
			true
		},
	},
	2,
	false // is playing
};


u8 chi = 0;
void playMUSIC(Music *iMusic, Settings *settings) {
	(*iMusic).isPlaying = true;
	for (chi=0;chi < (*iMusic).channelCount;chi++) {
		playSFX(&(*iMusic).ch[chi], &(*settings).music);
	}
}

void stopMUSIC(Music *iMusic) {
	(*iMusic).isPlaying = false;
	for (chi=0;chi < (*iMusic).channelCount;chi++) {
		SND_REGS[(*(*iMusic).ch).channel].SxINT = 0x00;
		(*(*iMusic).ch).isPlaying = false;
	}
}


void handleSfx(SFX *iSfx, u8 *audioLevel) {
	if ((*iSfx).isPlaying) {
		if ((*iSfx).freq_i < (*iSfx).freq_c) {
			(*iSfx).freqCounter++;
			if ((*iSfx).freqCounter >= (*iSfx).freq_l[(*iSfx).freq_i]) {
				(*iSfx).freq_i++;
				(*iSfx).freqCounter = 0;
				if ((*iSfx).freq_i >= (*iSfx).freq_c) {
					if ((*iSfx).isLooping == false) {
						SND_REGS[(*iSfx).channel].SxINT = 0x00; // stop channel
						SND_REGS[(*iSfx).channel].SxLRV = 0x00;
						(*iSfx).isPlaying = false;
					} else {
						playSFX(iSfx, audioLevel);
					}
				} else if ((*iSfx).freq_i < (*iSfx).freq_c) {
					if ((*iSfx).freq[(*iSfx).freq_i] == PAU || (*iSfx).freq[(*iSfx).freq_i] == NONE) {
						SND_REGS[(*iSfx).channel].SxINT = 0;
					} else {
						// play next note
						SND_REGS[(*iSfx).channel].SxINT = 0b10000000;
						//SND_REGS[(*iSfx).channel].SxLRV = (*audioLevel << 4) | *audioLevel;
						SND_REGS[(*iSfx).channel].SxFQL = (*iSfx).freq[(*iSfx).freq_i] & 0xFF;
						SND_REGS[(*iSfx).channel].SxFQH = (*iSfx).freq[(*iSfx).freq_i] >> 8;
					}

				}
			}
		}
		if ((*iSfx).env0_i < (*iSfx).env0_c) {
			(*iSfx).env0Counter++;

			if ((*iSfx).env0Counter >= (*iSfx).env0_l[(*iSfx).env0_i]) {
				(*iSfx).env0_i++;
				(*iSfx).env0Counter = 0;
				if ((*iSfx).isLooping == true && (*iSfx).env0_i >= (*iSfx).env0_c) {
					(*iSfx).env0_i = 0;
					SND_REGS[(*iSfx).channel].SxEV0 = (*iSfx).env0[(*iSfx).env0_i];
				} else if ((*iSfx).env0_i < (*iSfx).env0_c) {
					SND_REGS[(*iSfx).channel].SxEV0 = (*iSfx).env0[(*iSfx).env0_i];
				}
			}
		}
		if ((*iSfx).env1_i < (*iSfx).env1_c) {
			(*iSfx).env1Counter++;

			if ((*iSfx).env1Counter >= (*iSfx).env1_l[(*iSfx).env1_i]) {
				(*iSfx).env1_i++;
				(*iSfx).env1Counter = 0;
				if ((*iSfx).isLooping == true && (*iSfx).env1_i >= (*iSfx).env1_c) {
					(*iSfx).env1_i = 0;
					SND_REGS[(*iSfx).channel].SxEV1 = (*iSfx).env1[(*iSfx).env1_i];
				} else if ((*iSfx).env1_i < (*iSfx).env1_c) {
					SND_REGS[(*iSfx).channel].SxEV1 = (*iSfx).env1[(*iSfx).env1_i];
				}
			}
		}

	}
}

void handleMusic(Music *iMusic, Settings *settings) {
	for (chi=0;chi < (*iMusic).channelCount;chi++) {
		SND_REGS[(*iMusic).ch[chi].channel].SxLRV = ((*settings).music << 4) | (*settings).music;
		handleSfx(&(*iMusic).ch[chi], &(*settings).music);
	}
	//if ((*iMusic).isPlaying) {
		/*if ((*iMusic).ch1 != (void*)0) {
			handleSfx((*iMusic).ch1, settings);
		}*/
		//if ((*iMusic).ch2 != (void*)0) {
		//}
		/*
		if ((*iMusic).ch3 != (void*)0) {
			handleSfx((*iMusic).ch3, settings);
		}
		if ((*iMusic).ch4 != (void*)0) {
			handleSfx((*iMusic).ch4, settings);
		}
		if ((*iMusic).ch5 != (void*)0) {
			handleSfx((*iMusic).ch5, settings);
		}
		if ((*iMusic).ch6 != (void*)0) {
			handleSfx((*iMusic).ch6, settings);
		}*/
	//}
}

void audioLoop(Settings *settings) {

	handleMusic(&music, settings);
	//handleSfx(&(music.ch[0]), settings);
	handleSfx(&sfx, (*settings).sfx);
	handleSfx(&sfx2, (*settings).sfx);
	/*if ((*settings).music > 0) {

	} else {
		SND_REGS[0x00].SxINT = 0x00;
		SND_REGS[0x01].SxINT = 0x00;
		SND_REGS[0x02].SxINT = 0x00;
		SND_REGS[0x03].SxINT = 0x00;
		SND_REGS[0x04].SxINT = 0x00;
	}*/


/*
	if (isPlayingSfx) {
		SND_REGS[0x05].SxINT = 0xFF;
		//copy set values
		u8 vol = (*settings).sfx;
		u16 freq = 0x6AE;

		SND_REGS[0x05].SxLRV = (vol << 4) | vol;
		SND_REGS[0x05].SxFQL = freq & 0xFF;
		SND_REGS[0x05].SxFQH = freq >> 8;
		SND_REGS[0x05].SxEV0 = 0xff; // if fade: 0xf7
		SND_REGS[0x05].SxEV1 = 0x08;
		isPlayingSfx = 0;
	} else {
		SND_REGS[0x05].SxINT = 0x00;
	}*/
}

u8 optionsScreen(Settings *settings)
{
	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192); /* clear previous data */
	/* Full-screen world so border (upper-left etc.) is from BGMap(0), not garbage */
	vbSetWorld(31, WRLD_ON|0, 0, 0, 0, 0, 0, 0, 384, 224);
	vbSetWorld(30, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 32, 32); // skull
	vbSetWorld(29, WRLD_ON|2, 0, 0, 0, 0, 0, 0, 32, 32); // skull blacks
	vbSetWorld(28, WRLD_ON|3, 0, 0, 0, 0, 0, 0, 8, 8); // eyes

	vbSetWorld(27, WRLD_ON|4, 0, 0, 0, 0, 0, 0, 32, 32); // test...

	WA[26].head = WRLD_END;

	copymem((void*)0x00078000, (void*)title_screen_optionsTiles, 3296);  /* 206 tiles * 16 bytes */
	vbDisplayOn(); // turns the display on

	/* No offset: content is drawn at 4-tile offset in drawOptionsScreen so left border is 0 */
	WA[31].gy = 0;
	WA[31].gx = 0;

	VIP_REGS[GPLT0] = 0xE4;	/* Set 1st palettes to: 11100100 */
	VIP_REGS[GPLT1] = 0x00;	/* (i.e. "Normal" dark to light progression.) */
	VIP_REGS[GPLT2] = 0x84; // VIP_REGS[GPLT2] = 0xC2;

	VIP_REGS[GPLT3] = 0xA2;

	int x = 71; // starting x
	int y = 29; // starting y
	int shade = 3; // starting color (bright red)
	int parallax = 0; // starting parallax value
	int count; // counter used for pauses
	int pixelsToDraw = 0;

	drawOptionsScreen(1);
	clearOptionsBorder();
	/* Clear upper-left of BGMap(0) immediately before first frame to avoid garbage on hardware */
	setmem((void*)BGMap(0), 0, 64);
	vbDisplayShow(); // shows the display

	u8 pos = 0;
	u8 skullAnim = 0;
	u8 countSkullAnimUp = 1;
	u8 pushedButton = 0;
	u8 pushedButtonCounter = 0;
	u8 allowPushAgain = 0;
	/* initAudio() removed -- PCM waveforms already set by mp_init() in main.c */

	u8 pushedUp = 0;
	u8 shouldRumble = 0;
	u8 rumbleTimer = 0;
	u8 rumbleTime = 16;

	u16 keyInputs;

	/* Music is handled by the new musicplayer system (title music persists).
	 * No need to start/stop old synth music here. */

	while(1) {
		keyInputs = vbReadPad();

		if(keyInputs & (K_STA|K_A|K_B|K_SEL) && pos == 3  && allowPushAgain == 1) {
			playPlayerSFX(SFX_PISTOL);
			break;
		} else if(keyInputs & (K_RU|K_LU)) {
			if (allowPushAgain == 1) {
				allowPushAgain = 0;
				if (pos > 0) {
					playPlayerSFX(SFX_ELEVATOR_STP);
					pos--;
				}
			}
		} else if(keyInputs & (K_RD|K_LD)) {
			if (allowPushAgain == 1) {
				allowPushAgain = 0;
				if (pos < 3) {
					playPlayerSFX(SFX_ELEVATOR_STP);
					pos++;
				}
			}
		} else if(keyInputs & (K_RL|K_LL)) {
			if (allowPushAgain == 1) {
				if (pos == 0) {
					if ((*settings).sfx > 0) {
						(*settings).sfx--;
						playPlayerSFX(SFX_STONE_MOVE);
					}
			} else if (pos == 1) {
				if ((*settings).music > 0) {
					(*settings).music--;
					g_musicSetting = (*settings).music;
					g_musicVolume = musicVolFromSetting((*settings).music);
					if (g_musicVolume == 0) musicStop();
					playPlayerSFX(SFX_STONE_MOVE);
				}
			} else if (pos == 2) {
					if ((*settings).rumble > 0) {
						(*settings).rumble--;
						playPlayerSFX(SFX_STONE_MOVE);
						shouldRumble = 1;
						rumbleTimer = 0;
						if ((*settings).rumble > 0) {
							const RumbleLevel *rl = &g_rumbleLevels[(*settings).rumble - 1];
							rumble_setFrequency(rl->freq);
							rumble_playEffect(rl->effect);
						}
					}
				}
				allowPushAgain = 0;
			}
		}  else if(keyInputs & (K_RR|K_LR)) {
			if (allowPushAgain == 1) {
				if (pos == 0) {
					if ((*settings).sfx < 9) {
						(*settings).sfx++;
						playPlayerSFX(SFX_STONE_MOVE);
					}
			} else if (pos == 1) {
				if ((*settings).music < 9) {
					u8 wasMuted = (g_musicVolume == 0) ? 1 : 0;
					(*settings).music++;
					g_musicSetting = (*settings).music;
					g_musicVolume = musicVolFromSetting((*settings).music);
					if (wasMuted && g_musicVolume > 0) musicStart();
					playPlayerSFX(SFX_STONE_MOVE);
				}
			} else if (pos == 2) {
					if ((*settings).rumble < 9) {
						(*settings).rumble++;
						playPlayerSFX(SFX_STONE_MOVE);
						{
							const RumbleLevel *rl = &g_rumbleLevels[(*settings).rumble - 1];
							rumble_setFrequency(rl->freq);
							rumble_playEffect(rl->effect);
						}
						rumbleTimer = 0;
						shouldRumble = 1;
					}
				}
				allowPushAgain = 0;
			}
		} else { // let go of all buttons
			allowPushAgain = 1;
		}
/*
		if(buttonsPressed(K_STA|K_A|K_B|K_SEL, true)) {
			break;
		} else if(allowPushAgain == 1 && buttonsPressed(K_RU|K_LU, false)) {
			if (pos > 0) {
				pos--;
			}
			pushedButton = 1;
		} else if(allowPushAgain == 1 && buttonsPressed(K_RD|K_LD, false)) {
        	pos++;
        	if (pos > 3) {
        		pos = 3;
        	}
        	pushedButton = 1;
		}*/

		//drawDoomImage();
		drawOptionsSkull(skullAnim, pos);
		drawOptionsScreen(1);
		hideNumbers();
		drawNumbers(settings);


		if (shouldRumble) {
			rumbleTimer++;
			if (rumbleTimer > rumbleTime) {
				rumbleTimer = 0;
				shouldRumble = 0;
				rumble_stop();
			}
		}

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
		/* Tick the music sequencer so title music keeps playing */
		updateMusic(true);

		count++;
		vbWaitFrame(0);
	}
	/* Music channels (ch0/ch1/ch3) are kept alive by the sequencer */

	if (pos == 0) {
		vbFXFadeOut(0);
	}
	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192); /* clear previous data */
	return 0;
}