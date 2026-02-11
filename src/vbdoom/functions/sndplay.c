#include <audio.h>
#include <mem.h>
#include "sndplay.h"
#include "voices.h"
#include "dph9.h"
#include "timer.h"
#include "../assets/audio/doom_sfx.h"

/* Song data headers (static arrays, only included here) */
#include "../assets/audio/music_title.h"
#include "../assets/audio/music_e1m1.h"
#include "../assets/audio/music_e1m2.h"
#include "../assets/audio/music_e1m3.h"

/* ================================================================
 * Background Music -- 3-channel sequencer
 *
 * Ch 0 (WAVEDATA1, SAWTOOTH)  = melody
 * Ch 1 (WAVEDATA2, SQUARE)    = bass
 * Ch 3 (WAVEDATA4, TRIANGLE)  = chords
 *
 * All 3 channels share one timing array (lock-step advance).
 * Timing values are in milliseconds; actual timing measured via
 * g_musicTick (5 kHz ISR counter) for rate-independent playback.
 * ================================================================ */

#define MUS_CH_MELODY  0   /* VB sound channel index */
#define MUS_CH_BASS    1
#define MUS_CH_CHORDS  3

/* Master music volume (0-15).  Derived from settings.music (0-9). */
volatile u8 g_musicVolume = 8;

/* Sequencer state */
static const int   *mus_ch[3] = {0, 0, 0};  /* frequency arrays [melody, bass, chords] */
static const u16   *mus_timing   = 0;        /* shared timing (ms per step) */
static u16          mus_noteCount = 0;
static u16          mus_currNote  = 0;
static u8           mus_playing   = 0;
static u16          mus_prevFreq[3] = {0xFFFF, 0xFFFF, 0xFFFF}; /* per-channel */
static u32          mus_noteStart = 0;       /* g_musicTick when step started */

/* VB sound channel indices for our 3 music channels */
static const u8 mus_hw_ch[3] = { 0x00, 0x01, 0x03 };

/* ================================================================
 * PCM Sound Effects via SxLRV Volume Modulation
 *
 * Player sounds use channel 4 (SWEEP), modulated by timer ISR.
 * Enemy sounds use channel 2 (WAVE3), modulated by timer ISR.
 *
 * Technique (from DogP's wavonvb):
 *   - Waveform RAM filled with DC constant (0x3F = max 6-bit)
 *   - Channel plays the flat DC waveform at freq=0
 *   - PCM achieved by writing 4-bit samples to SxLRV per ISR tick
 *   - SxLRV acts as a 4-bit DAC (same value in L and R nibbles)
 * ================================================================ */

/* Convert distance byte (0-255) to volume (0-15). Closer = louder. */
static u8 distToVol(u8 distByte) {
	if (distByte < 8)   return 15;
	if (distByte < 16)  return 13;
	if (distByte < 32)  return 11;
	if (distByte < 64)  return 9;
	if (distByte < 100) return 7;
	if (distByte < 140) return 5;
	if (distByte < 200) return 3;
	return 1;
}

/* ----------------------------------------------------------------
 * Initialize sound system.
 *
 * Following the proven init_speech() pattern from wavonvb.c:
 *   1. SSTOP=1 to halt all sound
 *   2. Load DC waveform (0x3F) into waveform banks for PCM channels
 *   3. Load SAWTOOTH into WAVEDATA1 for music channel 0
 *   4. Turn off all channels
 *   5. SSTOP=0 to allow sound
 *   6. Configure PCM channels with DC waveform, freq=0, max envelope
 * ---------------------------------------------------------------- */
void mp_init(void) {
	int i;

	/* Stop all sound output */
	SSTOP = 1;

	/* Fill waveform RAM banks for PCM channels with DC constant.
	 * WAVEDATA entries are at 4-byte stride (byte writes). */
	for (i = 0; i < 32; i++) {
		WAVEDATA3[i << 2] = 0x3F;  /* for enemy ch2 */
		WAVEDATA5[i << 2] = 0x3F;  /* for player ch4 */
	}

	/* Load waveforms for music channels */
	copymem((void*)WAVEDATA1, (void*)SAWTOOTH, 128);  /* ch0 = melody */
	copymem((void*)WAVEDATA2, (void*)SQUARE,   128);  /* ch1 = bass */
	copymem((void*)WAVEDATA4, (void*)TRIANGLE, 128);  /* ch3 = chords */

	/* Turn off all channels */
	for (i = 0; i < 6; i++)
		SND_REGS[i].SxINT = 0x00;

	/* Allow sound output */
	SSTOP = 0;

	/* --- Configure music channels --- */
	SND_REGS[0x00].SxRAM = 0x00;  /* ch0: WAVEDATA1 (SAWTOOTH) */
	SND_REGS[0x01].SxRAM = 0x01;  /* ch1: WAVEDATA2 (SQUARE) */
	SND_REGS[0x03].SxRAM = 0x03;  /* ch3: WAVEDATA4 (TRIANGLE) */

	/* --- Configure PCM player channel (ch4 SWEEP) --- */
	SND_REGS[0x04].SxRAM = 0x04;  /* use WAVEDATA5 */
	SND_REGS[0x04].SxLRV = 0x00;  /* muted until playback */
	SND_REGS[0x04].SxEV0 = 0xF0;  /* max initial envelope, no step */
	SND_REGS[0x04].SxEV1 = 0x00;  /* no envelope modification */
	SND_REGS[0x04].SxFQL = 0x00;  /* freq=0 (irrelevant, DC waveform) */
	SND_REGS[0x04].SxFQH = 0x00;
	SND_REGS[0x04].S5SWP = 0x00;  /* disable sweep */
	SND_REGS[0x04].SxINT = 0x80;  /* enable channel */

	/* --- Configure PCM enemy channel (ch2 WAVE3) --- */
	SND_REGS[0x02].SxRAM = 0x02;  /* use WAVEDATA3 */
	SND_REGS[0x02].SxLRV = 0x00;  /* muted until playback */
	SND_REGS[0x02].SxEV0 = 0xF0;  /* max initial envelope, no step */
	SND_REGS[0x02].SxEV1 = 0x00;  /* no envelope modification */
	SND_REGS[0x02].SxFQL = 0x00;  /* freq=0 (irrelevant, DC waveform) */
	SND_REGS[0x02].SxFQH = 0x00;
	SND_REGS[0x02].SxINT = 0x80;  /* enable channel */
}

/* ----------------------------------------------------------------
 * Start PCM on the player channel (ch4 SWEEP).
 * Just sets up the stream state; the ISR handles SxLRV writes.
 * ---------------------------------------------------------------- */
void playPlayerSFX(u8 soundId) {
	if (soundId >= SFX_COUNT) return;

	/* Stop current playback to avoid ISR reading inconsistent state */
	g_pcmPlayer.playing = 0;
	SND_REGS[0x04].SxLRV = 0x00;

	/* Set up PCM stream state */
	g_pcmPlayer.data = sfx_table[soundId].data;
	g_pcmPlayer.length = sfx_table[soundId].length;
	g_pcmPlayer.cursor = 0;
	g_pcmPlayer.volume = 15;

	/* Start -- ISR will begin writing samples on next tick */
	g_pcmPlayer.playing = 1;
}

/* ----------------------------------------------------------------
 * Start PCM on the enemy channel (ch2 WAVE3).
 * ---------------------------------------------------------------- */
void playEnemySFX(u8 soundId, u8 distance) {
	u8 vol;
	if (soundId >= SFX_COUNT) return;

	vol = distToVol(distance);

	/* Stop current playback to avoid ISR reading inconsistent state */
	g_pcmEnemy.playing = 0;
	SND_REGS[0x02].SxLRV = 0x00;

	/* Set up PCM stream state */
	g_pcmEnemy.data = sfx_table[soundId].data;
	g_pcmEnemy.length = sfx_table[soundId].length;
	g_pcmEnemy.cursor = 0;
	g_pcmEnemy.volume = vol;

	/* Start -- ISR will begin writing samples on next tick */
	g_pcmEnemy.playing = 1;
}

/* ----------------------------------------------------------------
 * Music: load / start / stop / update
 * ---------------------------------------------------------------- */

/* Load a song by ID (SONG_TITLE, SONG_E1M1, etc.).
 * Does NOT start playback -- call musicStart() afterwards. */
void musicLoadSong(u8 songId) {
	/* Stop current playback first */
	musicStop();

	switch (songId) {
		case SONG_TITLE:
			mus_ch[0]     = music_title_melody;
			mus_ch[1]     = music_title_bass;
			mus_ch[2]     = music_title_chords;
			mus_timing    = music_title_timing;
			mus_noteCount = MUSIC_TITLE_NOTE_COUNT;
			break;
		case SONG_E1M1:
			mus_ch[0]     = music_e1m1_melody;
			mus_ch[1]     = music_e1m1_bass;
			mus_ch[2]     = music_e1m1_chords;
			mus_timing    = music_e1m1_timing;
			mus_noteCount = MUSIC_E1M1_NOTE_COUNT;
			break;
		case SONG_E1M2:
			mus_ch[0]     = music_e1m2_melody;
			mus_ch[1]     = music_e1m2_bass;
			mus_ch[2]     = music_e1m2_chords;
			mus_timing    = music_e1m2_timing;
			mus_noteCount = MUSIC_E1M2_NOTE_COUNT;
			break;
		case SONG_E1M3:
			mus_ch[0]     = music_e1m3_melody;
			mus_ch[1]     = music_e1m3_bass;
			mus_ch[2]     = music_e1m3_chords;
			mus_timing    = music_e1m3_timing;
			mus_noteCount = MUSIC_E1M3_NOTE_COUNT;
			break;
		default:
			mus_ch[0]     = 0;
			mus_ch[1]     = 0;
			mus_ch[2]     = 0;
			mus_noteCount = 0;
			break;
	}
}

/* Start (or restart) the currently loaded song. */
void musicStart(void) {
	if (!mus_ch[0] || mus_noteCount == 0) return;
	if (g_musicVolume == 0) return;  /* volume 0 = no music */

	mus_currNote    = 0;
	mus_prevFreq[0] = 0xFFFF;
	mus_prevFreq[1] = 0xFFFF;
	mus_prevFreq[2] = 0xFFFF;
	mus_noteStart   = g_musicTick;
	mus_playing     = 1;
}

/* Silence the 3 music hardware channels without touching sequencer state. */
static void musSilenceAll(void) {
	SND_REGS[0x00].SxINT = 0x00;  /* ch0 melody */
	SND_REGS[0x01].SxINT = 0x00;  /* ch1 bass */
	SND_REGS[0x03].SxINT = 0x00;  /* ch3 chords */
}

/* Stop playback, reset position, and silence all music channels. */
void musicStop(void) {
	mus_playing  = 0;
	mus_currNote = 0;
	musSilenceAll();
}

/* Helper: play or silence one VB sound channel. */
static void musPlayChannel(u8 hw_ch, u16 freq, u8 vol) {
	if (freq == 0) {
		SND_REGS[hw_ch].SxINT = 0x00;
	} else {
		SND_REGS[hw_ch].SxINT = 0x00;  /* reset first */
		SND_REGS[hw_ch].SxFQL = freq & 0xFF;
		SND_REGS[hw_ch].SxFQH = freq >> 8;
		SND_REGS[hw_ch].SxLRV = (vol << 4) | vol;
		SND_REGS[hw_ch].SxEV0 = 0xF0;  /* max envelope, no decay */
		SND_REGS[hw_ch].SxEV1 = 0x00;
		SND_REGS[hw_ch].SxINT = 0x9F;  /* enable, no interval */
	}
}

/* Update background music sequencer.  Call from any loop at any rate.
 * Drives 3 VB sound channels in lock-step.  Loops automatically.
 * Timing is rate-independent via g_musicTick (5 kHz ISR counter). */
void updateMusic(bool isPlayMusic) {
	u8 vol;
	u8 i;
	u32 elapsed;
	u32 dur_ticks;

	if (!isPlayMusic || !mus_playing || !mus_ch[0])
		return;

	vol = g_musicVolume;

	/* Volume 0: keep sequencer advancing but silence the channels */
	if (vol == 0) {
		/* Silence once (when transitioning from audible to muted) */
		if (mus_prevFreq[0] != 0xFFFE) {
			musSilenceAll();
			mus_prevFreq[0] = 0xFFFE; /* sentinel: muted */
			mus_prevFreq[1] = 0xFFFE;
			mus_prevFreq[2] = 0xFFFE;
		}
	} else {
		/* Update each channel if its frequency changed */
		for (i = 0; i < 3; i++) {
			u16 freq = mus_ch[i] ? (u16)mus_ch[i][mus_currNote] : 0;
			if (freq != mus_prevFreq[i]) {
				musPlayChannel(mus_hw_ch[i], freq, vol);
				mus_prevFreq[i] = freq;
			}
		}
	}

	/* Check if current step duration has elapsed (ms * 5 ticks/ms) */
	elapsed = g_musicTick - mus_noteStart;
	dur_ticks = (u32)mus_timing[mus_currNote] * 5;

	if (elapsed >= dur_ticks) {
		mus_currNote++;
		mus_noteStart = g_musicTick;

		/* Force re-trigger on next updateMusic call */
		mus_prevFreq[0] = 0xFFFF;
		mus_prevFreq[1] = 0xFFFF;
		mus_prevFreq[2] = 0xFFFF;

		/* Loop: restart when reaching end */
		if (mus_currNote >= mus_noteCount) {
			mus_currNote = 0;
		}
	}
}
