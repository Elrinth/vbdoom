#include "types.h"
#include "video.h"
#include "world.h"


WORLD* const WA = (WORLD*)0x0003D800;


/***** World Functions *****/
// (Obsoleted by the WORLD_* macros...)
void vbSetWorld (s16 nw, u16 header, u16 gx, s16 gp, u16 gy, u16 mx, s16 mp, u16 my, u16 width, u16 height)
{
	u16 tmp = nw << 4;

	WAM[tmp++] = header;
	WAM[tmp++] = gx;
	WAM[tmp++] = gp;
	WAM[tmp++] = gy;
	WAM[tmp++] = mx;
	WAM[tmp++] = mp;
	WAM[tmp++] = my;
	WAM[tmp++] = width;
	WAM[tmp] = height;
}