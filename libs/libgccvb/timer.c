#include "types.h"
#include "hw.h"
#include "timer.h"


u8 tcr_val = 0;

void timer_enable(int enb) 
{
	if (enb) tcr_val |= TIMER_ENB;
	else tcr_val &= ~TIMER_ENB;
	HW_REGS[TCR] = tcr_val;
}

u16 timer_get() 
{
	return (HW_REGS[TLR] | (HW_REGS[THR] << 8));
}

void timer_set(u16 time) 
{
	HW_REGS[TLR] = (time & 0xFF);
	HW_REGS[THR] = (time >> 8);
}

void timer_freq(int freq) 
{
	if (freq) tcr_val |= TIMER_20US;
	else tcr_val &= ~TIMER_20US;
	HW_REGS[TCR] = tcr_val;
}

void timer_int(int enb) 
{
	if (enb) tcr_val |= TIMER_INT;
	else tcr_val &= ~TIMER_INT;
	HW_REGS[TCR] = tcr_val;
}

int timer_getstat() 
{
	return (!!(HW_REGS[TCR] & TIMER_ZSTAT));
}

void timer_clearstat() 
{
	HW_REGS[TCR] = (tcr_val|TIMER_ZCLR);
}