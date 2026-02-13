#include <libgccvb.h>
#include <audio.h>
#include "timer.h"

/* ================================================================
 * PCM playback via SxLRV modulation + frame timing.
 *
 * Timer fires every 100us (reload=1, 100us base) = 10,000 Hz.
 * Each ISR tick writes ONE 4-bit sample to the SxLRV register
 * of each active PCM channel, acting as a 4-bit DAC.
 *
 * Waveform RAM holds a DC constant (0x3F); the actual audio
 * comes from modulating the volume register.
 *
 * CPU cost: ~150 cycles per ISR @ 10kHz = 1.5M/sec = 7.5% of 20MHz
 * ================================================================ */

/* Timer interval: 1 => 1 tick * 100us = 100us = 10,000 Hz */
#define PCM_TIMER_TICKS     1

/* Ticks per frame: 10000 Hz / 20 fps = 500 */
#define DEFAULT_TICKS_PER_FRAME  500

/* Flag set by ISR when frame time elapsed */
volatile u8 g_timerExpired = 0;

/* Frame timing: count ISR ticks */
static volatile u16 g_frameTick = 0;
static u16 g_ticksPerFrame = DEFAULT_TICKS_PER_FRAME;

/* Two independent PCM streams */
volatile PCMStream g_pcmPlayer = { 0, 0, 0, 0, 0 };
volatile PCMStream g_pcmEnemy  = { 0, 0, 0, 0, 0 };

/* Master SFX volume (0-15, default max). Updated by game from settings. */
volatile u8 g_sfxVolume = 15;

/* Music sequencer tick counter.  Incremented at 10,000 Hz by ISR.
 * Effective rate ~8 kHz due to ISR overhead.  ~8 ticks = 1 ms.
 * Used by updateMusic() for rate-independent timing. */
volatile u32 g_musicTick = 0;

void timerHandle()
{
	/* Disable timer interrupt and stop timer first (VB timer is one-shot;
	 * must do a clean disable -> re-enable cycle to restart). */
	timer_int(0);
	VIP_REGS[INTCLR] = VIP_REGS[INTPND];
	timer_enable(0);
	timer_clearstat();

	/* ---- Player PCM (ch4): write one 4-bit sample to SxLRV ---- */
	if (g_pcmPlayer.playing) {
		u16 idx = g_pcmPlayer.cursor;
		u8 packed = g_pcmPlayer.data[idx >> 1];
		u8 nib = (idx & 1) ? (packed & 0x0F) : (packed >> 4);
		nib = (nib * g_sfxVolume) >> 4;
		SND_REGS[0x04].SxLRV = (nib << 4) | nib;
		g_pcmPlayer.cursor++;
		if (g_pcmPlayer.cursor >= g_pcmPlayer.length) {
			g_pcmPlayer.playing = 0;
			SND_REGS[0x04].SxLRV = 0x00;
		}
	}

	/* ---- Enemy PCM (ch2): distance attenuation + master SFX volume ---- */
	if (g_pcmEnemy.playing) {
		u16 idx = g_pcmEnemy.cursor;
		u8 packed = g_pcmEnemy.data[idx >> 1];
		u8 nib = (idx & 1) ? (packed & 0x0F) : (packed >> 4);
		u8 scaled = (nib * g_pcmEnemy.volume) >> 4;
		scaled = (scaled * g_sfxVolume) >> 4;
		SND_REGS[0x02].SxLRV = (scaled << 4) | scaled;
		g_pcmEnemy.cursor++;
		if (g_pcmEnemy.cursor >= g_pcmEnemy.length) {
			g_pcmEnemy.playing = 0;
			SND_REGS[0x02].SxLRV = 0x00;
		}
	}

	/* ---- Music tick: simple 1:1 increment (10,000 Hz).
	 * Tempo scaling moved to updateMusic() multiplier. */
	g_musicTick++;

	/* ---- Frame timing ---- */
	g_frameTick++;
	if (g_frameTick >= g_ticksPerFrame) {
		g_frameTick = 0;
		g_timerExpired = 1;
	}

	/* Re-arm timer: set count, then restart with enable+interrupt */
	timer_set(PCM_TIMER_TICKS);
	timer_enable(1);
	timer_int(1);
	set_intlevel(0);
}

void setupTimer()
{
	/* Setup timer interrupt vector */
	timVector = (u32)(timerHandle);
	VIP_REGS[INTCLR] = VIP_REGS[INTPND];
	VIP_REGS[INTENB] = 0x0000;
	set_intlevel(0);
	INT_ENABLE;

	/* Configure hardware timer: 100us interval, 10kHz for PCM */
	timer_freq(TIMER_100US);
	timer_set(PCM_TIMER_TICKS);
	timer_clearstat();
	timer_int(1);
	timer_enable(1);
}

void waitForFrameTimer(void)
{
	/* Spin until enough ISR ticks have elapsed for one frame */
	while (!g_timerExpired) {
		/* busy-wait */
	}
	g_timerExpired = 0;
}

void setFrameTime(u16 ms)
{
	/* Convert milliseconds to ISR tick count.
	 * Each tick = 100us = 0.1ms, so ticks = ms * 10 */
	u16 ticks = ms * 10;
	if (ticks < 1) ticks = 1;
	g_ticksPerFrame = ticks;
}
