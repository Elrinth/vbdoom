#include <libgccvb.h>
#include "timer.h"

/* Flag set by interrupt handler when timer reaches zero */
volatile u8 g_timerExpired = 0;

/* Current target frame time in timer ticks (default: TIME_MS(50) = 499) */
static u16 g_frameTimeTicks = 499; /* TIME_MS(50) = (50*10)-1 = 499 */

void timerHandle()
{
	/* Disable interrupts and timer during handler */
	timer_int(0);
	VIP_REGS[INTCLR] = VIP_REGS[INTPND];
	timer_enable(0);

	/* Clear timer state and signal frame timer expired */
	timer_clearstat();
	g_timerExpired = 1;

	/* Re-enable timer for next interval */
	timer_set(g_frameTimeTicks);
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

	/* Configure hardware timer: 100us interval mode */
	timer_freq(TIMER_100US);
	timer_set(g_frameTimeTicks);
	timer_clearstat();
	timer_int(1);
	timer_enable(1);
}

void waitForFrameTimer(void)
{
	/* Spin until the timer interrupt fires */
	while (!g_timerExpired) {
		/* busy-wait */
	}
	g_timerExpired = 0;
}

void setFrameTime(u16 ms)
{
	/* Convert milliseconds to timer ticks: TIME_MS(n) = n*10 - 1 */
	g_frameTimeTicks = (ms * 10) - 1;
	/* Apply immediately for next timer cycle */
	timer_set(g_frameTimeTicks);
}
