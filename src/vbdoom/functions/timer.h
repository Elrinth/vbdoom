#ifndef _FUNCTIONS_TIMER_H
#define _FUNCTIONS_TIMER_H

#include <types.h>

void timerHandle();
void setupTimer();

/* Frame-rate capping: blocks until the target frame time has elapsed.
 * Uses the VB hardware timer (100us interval).
 * Call at the end of the game loop, after vbWaitFrame. */
void waitForFrameTimer(void);

/* Set the target frame time in milliseconds (default: 50ms = 20fps).
 * Common values: 50 = 20fps, 33 = ~30fps, 20 = 50fps */
void setFrameTime(u16 ms);

/* Read whether the timer has expired since last waitForFrameTimer call.
 * Useful for profiling: if expired before you call wait, frame was slow. */
extern volatile u8 g_timerExpired;

#endif
