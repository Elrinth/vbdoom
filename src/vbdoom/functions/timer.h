#ifndef _FUNCTIONS_TIMER_H
#define _FUNCTIONS_TIMER_H

#include <types.h>

/* ================================================================
 * PCM playback state -- accessed by both ISR and main thread.
 *
 * PCM is achieved by modulating the SxLRV (volume) register
 * with 4-bit packed sample data while a DC waveform plays.
 * The ISR fires at ~5,000 Hz and writes one sample per tick.
 * ================================================================ */
typedef struct {
	const u8 *data;    /* packed 4-bit sample pairs (high nibble first) */
	u16 cursor;        /* current sample index (not byte index) */
	u16 length;        /* total samples */
	u8 volume;         /* 0-15, for distance attenuation */
	u8 playing;        /* 0 = idle, 1 = playing */
} PCMStream;

/* Two independent PCM streams */
extern volatile PCMStream g_pcmPlayer;  /* channel 4 (SWEEP) */
extern volatile PCMStream g_pcmEnemy;   /* channel 2 (WAVE3) */

/* Master SFX volume (0-15). Derived from settings.sfx (0-9).
 * Applied in the timer ISR to scale all PCM output. */
extern volatile u8 g_sfxVolume;

/* Music sequencer tick counter.  Incremented at 5,000 Hz by ISR.
 * 5 ticks = 1 ms.  Read by updateMusic() for rate-independent timing. */
extern volatile u32 g_musicTick;

void timerHandle();
void setupTimer();

/* Frame-rate capping: blocks until the target frame time has elapsed.
 * The hardware timer fires at ~5,000 Hz for PCM sample output;
 * frame timing is derived by counting ISR ticks (250 per 20fps frame). */
void waitForFrameTimer(void);

/* Set the target frame time in milliseconds (default: 50ms = 20fps).
 * Common values: 50 = 20fps, 33 = ~30fps, 20 = 50fps */
void setFrameTime(u16 ms);

/* Read whether the timer has expired since last waitForFrameTimer call.
 * Useful for profiling: if expired before you call wait, frame was slow. */
extern volatile u8 g_timerExpired;

#endif
