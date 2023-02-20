#include <libgccvb.h>
#include "timer.h"

void timerHandle()
{
	//disable interrupts
	timer_int(0);
	VIP_REGS[INTCLR] = VIP_REGS[INTPND];
	//disable timer
	timer_enable(0);

	//clear timer state
	timer_clearstat();

	//enable timer
	timer_enable(1);
	//enable interrupts
	timer_int(1);

	set_intlevel(0);
}

void setupTimer()
{
	//setup timer interrupts
	//timerHandle(): function that gets called when the interrupt fires (e.g. when the timer reaches 0)
	timVector = (u32)(timerHandle);
	VIP_REGS[INTCLR] = VIP_REGS[INTPND];
	VIP_REGS[INTENB] = 0x0000; //this is only for enabling\disabling different kinds of vpu and error ints
	set_intlevel(0);
	INT_ENABLE;

	//setup timer
	timer_freq(TIMER_100US);
	timer_set(TIME_MS(50)); //timer will get fired every 50ms
	timer_clearstat();
	timer_int(1);
	timer_enable(1);
}