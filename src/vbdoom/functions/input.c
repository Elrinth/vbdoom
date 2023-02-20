#include <libgccvb.h>
#include <constants.h>
#include "input.h"

extern u16 lastPressedButtons;

u8 buttonsPressed(u16 buttons, u8 forceRelease)
{
	if(!(vbReadPad() & buttons & ~K_PWR)) {
		lastPressedButtons = 0;
	}

	if((vbReadPad() & buttons & ~K_PWR) && ((!forceRelease) || (lastPressedButtons != vbReadPad()))) {
		lastPressedButtons = vbReadPad();
		return true;
	} else {
		return false;
	}
}