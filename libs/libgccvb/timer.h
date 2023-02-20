#ifndef _LIBGCCVB_TIMER_H_
#define _LIBGCCVB_TIMER_H_


#include "types.h"
#include "hw.h"


//use with 20us timer (range = 0 to 1300)
#define TIME_US(n)		(((n)/20)-1)

//use with 100us timer (range = 0 to 6500, and 0 to 6.5)
#define TIME_MS(n)		(((n)*10)-1)
#define TIME_SEC(n)		(((n)*10000)-1)


#define TIMER_ENB		0x01
#define TIMER_ZSTAT		0x02
#define TIMER_ZCLR		0x04
#define TIMER_INT		0x08
#define TIMER_20US		0x10
#define TIMER_100US		0x00


void timer_enable(int enb);
u16 timer_get();
void timer_set(u16 time);
void timer_freq(int freq);
void timer_int(int enb);
int timer_getstat();
void timer_clearstat();


#endif