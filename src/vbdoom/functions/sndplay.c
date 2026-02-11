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
#include "../assets/audio/music_e1m5.h"
#include "../assets/audio/music_e1m6.h"

/* ================================================================
 * Background Music -- 4-channel sequencer
 *
 * Ch 0 (WAVEDATA1, PIANO)     = melody
 * Ch 1 (WAVEDATA2, SQUARE)    = bass
 * Ch 3 (WAVEDATA4, TRIANGLE)  = chords
 * Ch 5 (NOISE)                = drums
 *
 * PCM SFX use dedicated channels (ch2 and ch4) that carry no music,
 * so SFX never interrupts or mutes any music channel.
 *
 * All 4 channels share one timing array (lock-step advance).
 * Timing values are in milliseconds; actual timing measured via
 * g_musicTick (10 kHz ISR counter) for rate-independent playback.
 *
 * Tonal channel encoding (melody/bass/chords):
 *   bits 15-12 = velocity (0-15; 0 = silence/rest)
 *   bits 10-4  = MIDI note number (0-127)
 *   VB freq looked up via midi_freq[] table at runtime.
 *
 * Arpeggio (tracker-style 0xy effect, optional per song):
 *   One u8 per step. Upper nibble = semitone offset for 2nd note,
 *   lower nibble = offset for 3rd note. 0x00 = no arpeggio.
 *   Player cycles: base, base+hi, base+lo at ~50Hz (frame rate).
 *
 * Drum values encode noise parameters:
 *   bits 14-12 = tap register (LFSR feedback point, 0-7)
 *   bits 11-10 = envelope decay speed (0=very fast, 3=slow)
 *   bits  9-0  = noise frequency register
 *   0 = silence
 * ================================================================ */

#define MUS_NUM_CH     4   /* Total music channels */
#define MUS_CH_MELODY  0   /* VB sound channel index */
#define MUS_CH_BASS    1
#define MUS_CH_CHORDS  3
#define MUS_CH_DRUMS   5   /* Noise channel (hardware ch6, index 0x05) */

/* Arpeggio phase rate: ticks between note changes.
 * ISR runs at ~10kHz nominal (~7kHz effective).
 * 140 ticks ≈ 20ms ≈ 1 frame at 50Hz = C64 PAL arpeggio rate. */
#define ARP_PHASE_TICKS  140

/* MIDI note number to VB frequency register lookup table (128 entries).
 * Copied from dph9.h / convert_midi.py's proven VB note table.
 * Index = MIDI note number (0-127), value = VB SxFQL/SxFQH register. */
static const u16 midi_freq[128] = {
	/* 0-11: C-1 to B-1 (inaudible) */
	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
	/* 12-23: C0 to B0 */
	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
	/* 24-35: C1 to B1 */
	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
	/* 36-47: C2 to B2 */
	0x000,0x000,0x000,0x000,0x02C,0x09C,0x106,0x16B,0x1C9,0x223,0x277,0x2C6,
	/* 48-59: C3 to B3 */
	0x312,0x356,0x39B,0x3DA,0x416,0x44E,0x483,0x4B5,0x4E5,0x511,0x53B,0x563,
	/* 60-71: C4 to B4 */
	0x589,0x5AC,0x5CE,0x5ED,0x60A,0x627,0x642,0x65B,0x672,0x689,0x69E,0x6B2,
	/* 72-83: C5 to B5 */
	0x6C4,0x6D6,0x6E7,0x6F7,0x706,0x714,0x721,0x72D,0x739,0x744,0x74F,0x759,
	/* 84-95: C6 to B6 */
	0x762,0x76B,0x773,0x77B,0x783,0x78A,0x790,0x797,0x79D,0x7A2,0x7A7,0x7AC,
	/* 96-107: C7 to B7 */
	0x7B1,0x7B6,0x7BA,0x7BE,0x7C1,0x7C4,0x7C8,0x7CB,0x7CE,0x7D1,0x7D4,0x7D6,
	/* 108-119: C8+ */
	0x7D9,0x7DB,0x7DD,0x7DF,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
	/* 120-127: above range */
	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
};

/* Master music volume (0-4).  Derived from settings.music (0-9). */
volatile u8 g_musicVolume = 3;

/* Raw settings.music value (0-9) for pause-menu reconstruction */
u8 g_musicSetting = 5;

/* Sequencer state */
static const int   *mus_ch[MUS_NUM_CH] = {0, 0, 0, 0};  /* [melody, bass, chords, drums] */
static const u16   *mus_timing   = 0;        /* shared timing (ms per step) */
static const u8    *mus_arp      = 0;        /* arp offsets (NULL = no arp) */
static u8           mus_arpCh    = 0;        /* which tonal ch has arp (0-2) */
static u16          mus_noteCount = 0;
static u16          mus_currNote  = 0;
static u8           mus_playing   = 0;
static u16          mus_prevFreq[MUS_NUM_CH] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
static u32          mus_noteStart = 0;       /* g_musicTick when step started */

/* VB sound channel indices for our 4 music channels */
static const u8 mus_hw_ch[MUS_NUM_CH] = { 0x00, 0x01, 0x03, 0x05 };

/* Music volume lookup table: settings (0-9) → g_musicVolume.
 * VB waveform synthesis channels are ~4x louder than PCM SxLRV modulation,
 * plus 3 music channels play simultaneously vs 1 PCM channel.
 * This reduced non-linear curve (max 4) keeps music balanced with SFX. */
static const u8 musVolTable[10] = {0, 1, 1, 2, 2, 3, 3, 3, 4, 4};

u8 musicVolFromSetting(u8 setting) {
	if (setting > 9) setting = 9;
	return musVolTable[setting];
}

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
 *   3. Load PIANO/SQUARE/TRIANGLE into banks for music channels
 *   4. Turn off all channels
 *   5. SSTOP=0 to allow sound
 *   6. Configure PCM channels with DC waveform, freq=0, max envelope
 *   7. Configure noise channel (ch5) for drums
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

	/* Load waveforms for music channels.
	 * PIANO has richer harmonics than SAWTOOTH for lead melodies. */
	copymem((void*)WAVEDATA1, (void*)PIANO,    128);  /* ch0 = melody (PIANO) */
	copymem((void*)WAVEDATA2, (void*)SQUARE,   128);  /* ch1 = bass */
	copymem((void*)WAVEDATA4, (void*)TRIANGLE, 128);  /* ch3 = chords */
	/* ch5 (NOISE) doesn't use waveform RAM */

	/* Turn off all channels */
	for (i = 0; i < 6; i++)
		SND_REGS[i].SxINT = 0x00;

	/* Allow sound output */
	SSTOP = 0;

	/* --- Configure music channels --- */
	SND_REGS[0x00].SxRAM = 0x00;  /* ch0: WAVEDATA1 (PIANO) */
	SND_REGS[0x01].SxRAM = 0x01;  /* ch1: WAVEDATA2 (SQUARE) */
	SND_REGS[0x03].SxRAM = 0x03;  /* ch3: WAVEDATA4 (TRIANGLE) */

	/* --- Configure noise channel (ch5) for drums --- */
	/* Noise channel doesn't need SxRAM (no waveform bank).
	 * Initial state: disabled, will be configured per-hit by musPlayDrum(). */
	SND_REGS[0x05].SxINT = 0x00;  /* disabled until first drum hit */
	SND_REGS[0x05].SxLRV = 0x00;
	SND_REGS[0x05].SxEV0 = 0xF0;  /* max initial envelope */
	SND_REGS[0x05].SxEV1 = 0x00;  /* no tap/envelope mod yet */

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
 * Ch4 is a dedicated PCM channel -- no music plays there.
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
 * Ch2 is a dedicated PCM channel -- no music plays there.
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

	mus_arp    = 0;  /* default: no arpeggio */
	mus_arpCh  = 0;

	switch (songId) {
		case SONG_TITLE:
			mus_ch[0]     = music_title_melody;
			mus_ch[1]     = music_title_bass;
			mus_ch[2]     = music_title_chords;
			mus_ch[3]     = music_title_drums;
			mus_timing    = music_title_timing;
			mus_noteCount = MUSIC_TITLE_NOTE_COUNT;
#if MUSIC_TITLE_HAS_ARP
			mus_arp       = music_title_arp;
			mus_arpCh     = 2;  /* chords channel has arp */
#endif
			break;
		case SONG_E1M1:
			mus_ch[0]     = music_e1m1_melody;
			mus_ch[1]     = music_e1m1_bass;
			mus_ch[2]     = music_e1m1_chords;
			mus_ch[3]     = music_e1m1_drums;
			mus_timing    = music_e1m1_timing;
			mus_noteCount = MUSIC_E1M1_NOTE_COUNT;
#if MUSIC_E1M1_HAS_ARP
			mus_arp       = music_e1m1_arp;
			mus_arpCh     = 0;  /* melody channel has arp */
#endif
			break;
		case SONG_E1M2:
			mus_ch[0]     = music_e1m2_melody;
			mus_ch[1]     = music_e1m2_bass;
			mus_ch[2]     = music_e1m2_chords;
			mus_ch[3]     = music_e1m2_drums;
			mus_timing    = music_e1m2_timing;
			mus_noteCount = MUSIC_E1M2_NOTE_COUNT;
			break;
		case SONG_E1M3:
			mus_ch[0]     = music_e1m3_melody;
			mus_ch[1]     = music_e1m3_bass;
			mus_ch[2]     = music_e1m3_chords;
			mus_ch[3]     = music_e1m3_drums;
			mus_timing    = music_e1m3_timing;
			mus_noteCount = MUSIC_E1M3_NOTE_COUNT;
			break;
		case SONG_E1M5:
			mus_ch[0]     = music_e1m5_melody;
			mus_ch[1]     = music_e1m5_bass;
			mus_ch[2]     = music_e1m5_chords;
			mus_ch[3]     = music_e1m5_drums;
			mus_timing    = music_e1m5_timing;
			mus_noteCount = MUSIC_E1M5_NOTE_COUNT;
			break;
		case SONG_E1M6:
			mus_ch[0]     = music_e1m6_melody;
			mus_ch[1]     = music_e1m6_bass;
			mus_ch[2]     = music_e1m6_chords;
			mus_ch[3]     = music_e1m6_drums;
			mus_timing    = music_e1m6_timing;
			mus_noteCount = MUSIC_E1M6_NOTE_COUNT;
			break;
		default:
			mus_ch[0]     = 0;
			mus_ch[1]     = 0;
			mus_ch[2]     = 0;
			mus_ch[3]     = 0;
			mus_noteCount = 0;
			break;
	}
}

/* Start (or restart) the currently loaded song. */
void musicStart(void) {
	u8 i;
	if (!mus_ch[0] || mus_noteCount == 0) return;
	if (g_musicVolume == 0) return;  /* volume 0 = no music */

	mus_currNote  = 0;
	for (i = 0; i < MUS_NUM_CH; i++) {
		/* Arp channel uses freqForCmp=0xFFFF to always re-trigger, so its
		 * prevFreq must differ (0xFFFE) to ensure the first frame plays. */
		mus_prevFreq[i] = (mus_arp && i == mus_arpCh) ? 0xFFFE : 0xFFFF;
	}
	mus_noteStart = g_musicTick;
	mus_playing   = 1;
}

/* Silence all music hardware channels without touching sequencer state. */
static void musSilenceAll(void) {
	SND_REGS[0x00].SxINT = 0x00;  /* ch0 melody */
	SND_REGS[0x01].SxINT = 0x00;  /* ch1 bass */
	SND_REGS[0x03].SxINT = 0x00;  /* ch3 chords */
	SND_REGS[0x05].SxINT = 0x00;  /* ch5 noise/drums */
}

/* Stop playback, reset position, and silence all music channels. */
void musicStop(void) {
	mus_playing  = 0;
	mus_currNote = 0;
	musSilenceAll();
}

/* Helper: play or silence one VB waveform sound channel (melody/bass/chords).
 * freq:  VB frequency register value (0 = silence)
 * vol:   combined volume (velocity * master / 15), 0-15 */
static void musPlayChannel(u8 hw_ch, u16 freq, u8 vol) {
	if (freq == 0 || vol == 0) {
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

/* Helper: play a drum hit on the noise channel (ch5 / hw 0x05).
 *
 * drumVal encoding: (tap << 12) | (decay << 10) | freq
 *   bits 14-12 = tap register (LFSR feedback point, 0-7)
 *                Tap controls timbre via LFSR sequence length:
 *                0=32767(white noise) 1=1953 2=254 3=217
 *                4=73 5=63 6=42 7=28(tonal)
 *   bits 11-10 = envelope decay speed (0=very fast, 3=slow)
 *   bits  9-0  = noise frequency register
 *   0 = silence
 *
 * vol: master volume (0-15). */
static void musPlayDrum(u16 drumVal, u8 vol) {
	u8 tap;
	u8 decay;
	u16 freq;
	u8 ev0;

	if (drumVal == 0 || vol == 0) {
		SND_REGS[0x05].SxINT = 0x00;
		return;
	}

	tap   = (drumVal >> 12) & 0x07;
	decay = (drumVal >> 10) & 0x03;
	freq  = drumVal & 0x3FF;

	/* Slightly boost drum volume so they cut through the mix on VB hardware */
	if (vol < 15) {
		u16 d = (u16)vol * 14 / 10;
		vol = (u8)(d > 15 ? 15 : d);
	}

	/* SxEV0: bits 7-4 = initial volume (0xF = max),
	 *        bit  3   = envelope direction (0 = decay),
	 *        bits 2-0 = step interval (0=fastest ... 7=slowest).
	 * We use decay value (0-3) to set step interval for different drum types:
	 *   kick/snare:  fast decay (0-1)
	 *   hihats:      very fast (0)
	 *   open hihat:  medium (2)
	 *   crash/ride:  slow (3) */
	ev0 = 0xF0 | (decay + 1);  /* initial=15, decay dir=0, interval=decay+1 */

	SND_REGS[0x05].SxEV0 = ev0;
	SND_REGS[0x05].SxEV1 = (tap << 4) | 0x01;  /* tap bits + enable envelope */
	SND_REGS[0x05].SxFQL = freq & 0xFF;
	SND_REGS[0x05].SxFQH = (freq >> 8) & 0x03;
	SND_REGS[0x05].SxLRV = (vol << 4) | vol;
	SND_REGS[0x05].SxINT = 0x80;  /* enable channel (resets LFSR / re-triggers) */
}

/* Update background music sequencer.  Call from any loop at any rate.
 * Drives 4 VB sound channels in lock-step.  Loops automatically.
 * Timing is rate-independent via g_musicTick (10 kHz ISR counter).
 *
 * Tonal channels decode MIDI note -> VB freq via midi_freq[] lookup.
 * Arpeggio channels cycle through 3 notes at ~50Hz (ARP_PHASE_TICKS). */
void updateMusic(bool isPlayMusic) {
	u8 vol;
	u8 i;
	u32 elapsed;
	u32 dur_ticks;

	if (!isPlayMusic || !mus_playing || !mus_ch[0])
		return;

	vol = g_musicVolume;
	elapsed = g_musicTick - mus_noteStart;

	/* Volume 0: keep sequencer advancing but silence the channels */
	if (vol == 0) {
		/* Silence once (when transitioning from audible to muted) */
		if (mus_prevFreq[0] != 0xFFFE) {
			musSilenceAll();
			for (i = 0; i < MUS_NUM_CH; i++)
				mus_prevFreq[i] = 0xFFFE; /* sentinel: muted */
		}
	} else {
		/* Update melody, bass, chords (channels 0-2) */
		for (i = 0; i < 3; i++) {
			if (mus_ch[i]) {
				u16 raw = (u16)mus_ch[i][mus_currNote];
				u8  vel4 = (raw >> 12) & 0x0F;
				u8  midiNote = (raw >> 4) & 0x7F;
				u16 freq;
				u16 freqForCmp;  /* value to compare for change detection */

				/* Arpeggio: if this channel has arp data, cycle through
				 * 3 notes at frame rate using elapsed ISR ticks. */
				if (mus_arp && i == mus_arpCh) {
					u8 arpByte = mus_arp[mus_currNote];
					if (arpByte != 0 && midiNote > 0 && vel4 > 0) {
						u8 phase = (u8)((elapsed / ARP_PHASE_TICKS) % 3);
						if (phase == 1)
							midiNote += (arpByte >> 4) & 0x0F;
						else if (phase == 2)
							midiNote += arpByte & 0x0F;
						if (midiNote > 127) midiNote = 127;
					}
					/* For arp channels, always re-trigger (phase changes
					 * within a step without raw changing) */
					freqForCmp = 0xFFFF;
				} else {
					freqForCmp = raw;
				}

				if (freqForCmp != mus_prevFreq[i]) {
					if (vel4 == 0 || midiNote == 0) {
						musPlayChannel(mus_hw_ch[i], 0, 0);
					} else {
						u8 chVol;
						freq = midi_freq[midiNote];
						if (freq == 0) {
							musPlayChannel(mus_hw_ch[i], 0, 0);
						} else {
							chVol = (vel4 * vol) / 15;
							if (chVol == 0) chVol = 1;
							musPlayChannel(mus_hw_ch[i], freq, chVol);
						}
					}
					mus_prevFreq[i] = raw;
				}
			}
		}

		/* Update drums (channel 3 in our array, hw ch5 NOISE) */
		if (mus_ch[3]) {
			u16 raw = (u16)mus_ch[3][mus_currNote];
			if (raw != mus_prevFreq[3]) {
				musPlayDrum(raw, vol);
				mus_prevFreq[3] = raw;
			}
		}
	}

	/* Advance through steps whose duration has fully elapsed.
	 * Using a while-loop so multiple short steps (e.g. 11-42ms) can be
	 * consumed in a single frame (~50ms at 20fps), keeping the sequencer
	 * in sync with real time.  Overshoot is preserved by adding dur_ticks
	 * to mus_noteStart instead of resetting it to g_musicTick. */
	{
		u8 skipCount = 0;
		dur_ticks = (u32)mus_timing[mus_currNote] * 8;

		while (elapsed >= dur_ticks) {
			mus_noteStart += dur_ticks;  /* preserve overshoot */
			mus_currNote++;
			if (mus_currNote >= mus_noteCount)
				mus_currNote = 0;

			/* Force re-trigger on next channel update */
			for (i = 0; i < MUS_NUM_CH; i++)
				mus_prevFreq[i] = (mus_arp && i == mus_arpCh) ? 0xFFFE : 0xFFFF;

			elapsed = g_musicTick - mus_noteStart;
			dur_ticks = (u32)mus_timing[mus_currNote] * 8;

			if (++skipCount > 8) break;  /* safety: max 8 steps per frame */
		}
	}
}
