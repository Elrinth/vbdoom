#include "types.h"
#include "audio.h"

u8* const WAVEDATA1 =		(u8*)0x01000000; // 0x01000000 - 0x0100007F	Waveform 1 RAM
u8* const WAVEDATA2 =		(u8*)0x01000080; // 0x01000080 - 0x010000FF	Waveform 2 RAM
u8* const WAVEDATA3 =		(u8*)0x01000100; // 0x01000100 - 0x0100017F	Waveform 3 RAM
u8* const WAVEDATA4 =		(u8*)0x01000180; // 0x01000180 - 0x010001FF	Waveform 4 RAM
u8* const WAVEDATA5 =		(u8*)0x01000200; // 0x01000200 - 0x0100027F	Waveform 5 RAM
u8* const MODDATA =			(u8*)0x01000280;
SOUNDREG* const SND_REGS =	(SOUNDREG*)0x01000400;