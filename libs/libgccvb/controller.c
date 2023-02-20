#include "types.h"
#include "hw.h"
#include "controller.h"


u16 oldkeydown = 0x0000;
u16 oldkeyup = 0x0000;


/* Reads the keypad, returns the 16 button bits */
u16 vbReadPad()
{
	HW_REGS[SCR] = (S_INTDIS | S_HW);
	while (HW_REGS[SCR] & S_STAT);
	return (HW_REGS[SDHR] << 8) | HW_REGS[SDLR];
}

/* Check if a button has been pressed since the last read. If button state matches last read, it is returned as 'off' */
u16 vbPadKeyDown() 
{
	u16 keystate,keydown;

	keystate = vbReadPad() & K_ANY;
	keydown = (oldkeydown & keystate) ^ keystate;
	oldkeydown = keystate;

	return keydown;
}

/* Check if a button has been released since the last read. If button state matches last read, it is returned as 'off'  */
u16 vbPadKeyUp() 
{
	u16 keystate,keyup;

	keystate = vbReadPad() & K_ANY;
	keyup = (oldkeyup & ~keystate);
	oldkeyup = keystate;

	return keyup;
}