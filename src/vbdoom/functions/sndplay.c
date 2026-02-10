#include <audio.h>
#include "sndplay.h"
#include "voices.h"
#include "dph9.h"

//int batman_channel1[] = {E_3, PAU, E_3, PAU, E_4, E_3, E_3, D_4, E_3, E_3, C_4, CS5, D_5, E_5, FS5, A_5, G_5, FS5, E_5, C_5, G_4, C_5, D_5, E_5, FS5, G_5, A_5, G_5, FS5, E_5, E_5, PAU, G_5, FS5, E_5, FS5, E_5, D_5, CS5, PAU, B_4, FS5, D_5, G_5, FS5, D_5, B_4, AS4, B_4, CS5, D_5, E_5, FS5, A_5, G_5, FS5, E_5, C_5, G_4, C_5, D_5, E_5, FS5, G_5, A_5, G_5, FS5, E_5, E_5, PAU, FS5, E_5, FS5, D_5, E_5, CS5, D_5, PAU, B_4, PAU, FS5, B_5, CS6, D_6, B_5, D_6, CS6, D_6, E_6, A_5, A_5, B_5, CS6, D_6, B_5, G_5, B_5, G_5, D_5, G_5, D_5, B_4, D_5, B_4, G_4, G_5, G_4, A_4, G_4, FS4, CS5, FS5, CS6, FS6, E_6, D_6, E_6, D_6, CS6, B_5, CS6, B_5, AS5, GS5, AS5, B_5, B_5, FS5, CS6, CS6, FS5, D_6, D_6, FS5, E_6, D_6, E_6, FS6, B_5, B_5, G_5, CS6, CS6, G_5, D_6, D_6, G_5, E_6, D_6, E_6, FS6, CS6, CS6, A_5, D_6, D_6, A_5, E_6, E_6, A_5, FS6, E_6, FS6, G_6, FS6, E_6, CS6, D_6, CS6, PAU, D_6, E_6, FS6, E_6, D_6, CS6, B_5, B_5, FS5, CS6, CS6, FS5, D_6, D_6, FS5, E_6, D_6, E_6, FS6, B_5, B_5, G_5, CS6, CS6, G_5, D_6, D_6, G_5, E_6, D_6, E_6, FS6, E_6, E_6, A_5, FS6, FS6, A_5, G_6, G_6, A_5, A_6, G_6, A_6, FS6, G_6, E_6, A_5, AS5, B_5, };

//u8 batman_timing[] = {7, 7, 7, 7, 7, 7, 126, 21, 14, 14, 14, 7, 154, 21, 14, 14, 21, 21, 133, 7, 7, 7, 7, 7, 7, 7, 119, 28, 21, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 126, 21, 14, 14, 14, 7, 154, 21, 14, 14, 21, 21, 133, 7, 7, 7, 7, 7, 7, 7, 119, 28, 21, 7, 7, 7, 7, 7, 7, 7, 7, 1, 18, 1, 21, 14, 14, 7, 21, 14, 21, 21, 14, 7, 14, 21, 14, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 28, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 28, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 21, 21, 21, 14, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 28, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 28, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 56, 56, 225, };

//u8 batman_timing[] =   {3, 1, 3, 1, 4, 2,  21,  5,  2,  2,  2, 2,  31,  5,  2,  2,  5,  5,  24, 2, 2, 2, 2, 2, 2, 2,  17,  8,  5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,   8,  5,  2,  2,  2, 2,   8,  8,  2,  2,  5,  2,  18, 2, 2, 2, 2, 2, 2, 2,  18,  8,  5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 5,  2,  5,  2,  2, 2,  5,  2,  5,  5,  2, 2,  2,  5,  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  5,  5,  5,  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 8,   8, 14, };
//u16 batman_noteCount = 218;

u16 batman_channel1[] = {E_3, PAU, E_3, PAU, E_4, E_3, PAU, E_3, PAU, D_4, E_3, PAU, E_3, PAU, C_4, E_3, PAU, E_3, PAU, AS3, E_3, PAU, E_3, PAU, B_3, C_4, E_3, PAU, E_3, PAU, E_4, E_3, PAU, E_3, PAU, D_4, E_3, PAU, E_3, PAU, C_4, E_3, PAU, E_3, PAU,  AS3 };
u8 batman_timing[] =   {   3,   1,   3,   1,   4,   3,   1,   3,   1,   4,   3,   1,   3,   1,   4,  3,    1,   3,   1,   4,   3,   1,   3,   1,   4,   4,   3,   1,   3,   1,   4,   3,   1,   3,   1,   4,   3,   1,   3,   1,   4,  3,    1,   3,   1,   20 };
//int batman_channel6[] = {E_3, PAU, E_3, PAU, E_4, E_3, PAU, E_3, PAU, D_4, E_3, PAU, E_3, PAU, C_4, E_3, PAU, E_3, PAU, AS3, E_3, PAU, E_3, PAU, B_3, C_4, E_3, PAU, E_3, PAU, E_4, E_3, PAU, E_3, PAU, D_4, E_3, PAU, E_3, PAU, C_4, E_3, PAU, E_3, PAU,  AS3 };
u16 batman_channel6[] = {E_4, PAU, DS7, PAU, };
u8 batman_timing6[] =   {   2, 6,  1,   7,   };
u8 batman_noteCount = 26+26-6;
u8 channel6_noteCount = 4;

bool isPlaying = false;
u8 playMusic = 1;
u8 currentTick = 0;

u8 musicTick = 0;
u8 channel6Tick = 0;
u8 channel6Note = 0;
u8 channel6PrevNote = 99;
u8 musicCurrentNote = 0;
u8 musicPrevNote = 99;
u8 vol = 8;

/* Forward declaration for pickup sound update */
static void updatePickupSound(void);

void playSnd(bool *isFiring, u8 *iWeaponType, bool *isPlayMusic) {
	/*
	**** Sound Register Mnemonics *****/
	/*
	#define	WAVE1	0x00	// Voluntary wave channel #1
	#define	WAVE2	0x01	// Voluntary wave channel #2
	#define	WAVE3	0x02	// Voluntary wave channel #3
	#define	WAVE4	0x03	// Voluntary wave channel #4
	#define	SWEEP	0x04	// Sweep/modulation channel
	#define	NOISE	0x05	// Pseudorandom noise channel
	*/
	//enable sound
	if (*isFiring) {
		if (currentTick < 6) {
			SND_REGS[0x04].SxINT = 0x9F;
			//copy set values
			vol = 8 - (currentTick>>1);
			u16 freq = 0x72A-(currentTick<<7);
			if (*iWeaponType == 1) {
				freq = 0x42A-(currentTick<<7);
			}
			SND_REGS[0x04].SxLRV = (vol << 4) | vol;
			SND_REGS[0x04].SxFQL = freq & 0xFF;
			SND_REGS[0x04].SxFQH = freq >> 8;
/*
			if (iWeaponType == 1) {
				SND_REGS[0x04].SxEV0 = 0xaa-(currentTick<<8); // if fade: 0xf7
			} else {
				SND_REGS[0x04].SxEV0 = 0xfa-(currentTick<<8); // if fade: 0xf7
			}*/
			SND_REGS[0x04].SxEV0 = 0xaa;
			SND_REGS[0x04].SxEV1 = 0x00; // if fade: 0x01
			//SND_REGS[0x04].SxEV1 = 0x42;
			//SND_REGS[0x04].SxRAM = 0x04;
			//SND_REGS[0x04].S5SWP = 0xff-(currentTick<<8);
			//SND_REGS[0x04].SxRAM = (void*)CRAP;
		}

		if (currentTick > 6) {
			// make all channels silent:
			//for (count = 0; count < 6; count++)
				//
			SND_REGS[0x04].SxINT = 0x00;
		}
		currentTick++;
		if (currentTick > 24) {
			currentTick = 0;
		}
	} else {
		currentTick = 0;
		//for (count = 0; count < 6; count++)
			SND_REGS[0x04].SxINT = 0x00;
	}

	if (*isPlayMusic == 1) {
		u16 freq;
		if (musicCurrentNote != musicPrevNote) {
			freq = batman_channel1[musicCurrentNote];
			if (freq == PAU || freq == NONE) {
				SND_REGS[0x00].SxINT = 0x00;
			} else {
				SND_REGS[0x00].SxINT = 0x9F;
				//copy set values
				vol = 8;

				SND_REGS[0x00].SxLRV = (vol << 4) | vol;
				SND_REGS[0x00].SxFQL = freq & 0xFF;
				SND_REGS[0x00].SxFQH = freq >> 8;
				SND_REGS[0x00].SxEV0 = 0x99; // if fade: 0xf7
				SND_REGS[0x00].SxEV1 = 0x01;
			}
/*
			if (freq == PAU || freq == NONE) {
				SND_REGS[0x01].SxINT = 0x00;
			} else {
				SND_REGS[0x01].SxINT = 0x9F;
				//copy set values
				u8 vol = 3;

				SND_REGS[0x01].SxLRV = (vol << 4) | vol;
				SND_REGS[0x01].SxFQL = freq & 0xFF;
				SND_REGS[0x01].SxFQH = freq >> 8;
				SND_REGS[0x01].SxEV0 = 0x99; // if fade: 0xf7
				SND_REGS[0x01].SxEV1 = 0x01;
				SND_REGS[0x01].SxRAM = 0x01;
			}*/

			musicPrevNote = musicCurrentNote;
		}
		if (channel6Note != channel6PrevNote) {
			freq = batman_channel6[channel6Note];
			if (freq == PAU || freq == NONE) {
				SND_REGS[0x05].SxINT = 0x00;
			} else {
				SND_REGS[0x05].SxINT = 0xFF;
				//copy set values
				vol = 8;

				SND_REGS[0x05].SxLRV = (vol << 4) | vol;
				SND_REGS[0x05].SxFQL = freq & 0xFF;
				SND_REGS[0x05].SxFQH = freq >> 8;
				SND_REGS[0x05].SxEV0 = 0xff; // if fade: 0xf7
				SND_REGS[0x05].SxEV1 = 0x08;
			}
			channel6PrevNote = channel6Note;
		}
	}

	musicTick++;
	channel6Tick++;

	if (channel6Tick >= batman_timing6[channel6Note]) {
		channel6Note++;
		channel6Tick = 0;
	}
	if (channel6Note >= channel6_noteCount) {
		channel6Note = 0;
	}


	if (musicTick >= batman_timing[musicCurrentNote])
	{
		musicCurrentNote++;
		musicTick = 0;
	}
	if (musicCurrentNote >= batman_noteCount) {
		musicCurrentNote = 0;
	}

	/* Update pickup sound each frame */
	updatePickupSound();
}

/* Pickup sound state */
static u8 pickupSndTick = 0;
static bool pickupSndPlaying = false;

void playPickupSound(void) {
	pickupSndTick = 0;
	pickupSndPlaying = true;
}

void mp_init()
{
	copymem((void*)WAVEDATA1, (void*)SAWTOOTH, 128);
	/* Load SQUARE waveform for pickup sound on channel 1 (WAVE2) */
	copymem((void*)WAVEDATA2, (void*)SQUARE, 128);
}

/* Called from playSnd() each frame to handle pickup sound */
static void updatePickupSound(void) {
	if (!pickupSndPlaying) return;

	if (pickupSndTick < 4) {
		/* Rising blip: C6 -> E6 (high, cheerful, like Doom pickup) */
		u16 freq;
		u8 vol;
		if (pickupSndTick < 2) {
			freq = C_6;  /* first 2 frames: C6 */
			vol = 8;
		} else {
			freq = E_6;  /* next 2 frames: E6 (major third up) */
			vol = 6;
		}
		SND_REGS[0x01].SxINT = 0x9F;       /* enable channel 1 */
		SND_REGS[0x01].SxLRV = (vol << 4) | vol;
		SND_REGS[0x01].SxFQL = freq & 0xFF;
		SND_REGS[0x01].SxFQH = freq >> 8;
		SND_REGS[0x01].SxEV0 = 0x88;       /* mid envelope */
		SND_REGS[0x01].SxEV1 = 0x01;       /* sustain */
		SND_REGS[0x01].SxRAM = 0x01;       /* WAVEDATA2 */
	} else {
		/* Done */
		SND_REGS[0x01].SxINT = 0x00;       /* silence channel */
		pickupSndPlaying = false;
	}
	pickupSndTick++;
}
/*
const signed long SINE

 const signed long SAWTOOTH
 const signed long TRIANGLE
 const signed long SQUARE

 const signed long PIANO[

 const signed long TRUMPET[

 const signed long VIOLIN

 const signed long CRAP[]
 */