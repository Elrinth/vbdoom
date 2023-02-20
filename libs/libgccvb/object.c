#include "types.h"
#include "video.h"
#include "object.h"


// (Obsoleted by the OBJ_* macros...)
void vbSetObject (u16 n, u8 header, s16 x, s16 p, s16 y, u16 chr)
{
	n <<= 2;
	OAM[n    ] = (u16)x;
	OAM[n + 1] = (u16)p | ((u16)header << 8);
	OAM[n + 2] = (u16)y;
	OAM[n + 3] = ((header & 0x3C) << 10) | (chr & 0x7FF);
}