#ifndef _SFX_H_
#define _SFX_H

#include <stdbool.h>

// u8 = 0->255
typedef struct {
	u16* freq;
	u8* freq_l;
	u8 freq_c; // total number of 1 of the above

	u8* env0;
	u8* env0_l;
	u8 env0_c; // total number of 1 of the above

	u8* env1;
	u8* env1_l;
	u8 env1_c; // total number of 1 of the above

	bool isPlaying; // true / false

	u8 freq_i;
	u8 env0_i;
	u8 env1_i;

	u8 freqCounter;
	u8 env0Counter;
	u8 env1Counter;

	u8 channel;
	bool isLooping;
} SFX;

typedef struct {
	SFX* ch;
	u8 channelCount; // number of those above
	bool isPlaying;
} Music;

#endif