#include <libgccvb.h>
#include <mem.h>
#include <video.h>
#include <world.h>
#include <constants.h>
#include <languages.h>
#include "doominterface.h"

extern BYTE vb_doom_interfaceTiles[];
extern BYTE vb_doom_interfaceMap[];


//	Total size: 2352 + 384 = 2736
void drawDoomInterface(u8 bgmap, u16 x, u16 y)
{
	u16 pos=0;
	pos = (y << 7) + x;

	//copymem (u8* dest, const u8* src, u16 num)
	copymem((void*)(CharSeg0), (void*)vb_doom_interfaceTiles, 2352);
	//vbSetWorld(29, WRLD_ON, 0, 0, 192, 0, 0, 0, 384, 32);
	//BGMM[(0x1000 * bgmap) + pos] = (u8)t_string[i] + 0x700;
	copymem((void*)BGMap(0x1000 * bgmap)+pos, (void*)(vb_doom_interfaceMap), 96);
	copymem((void*)BGMap(0x1000 * bgmap)+pos+128, (void*)(vb_doom_interfaceMap+96), 96);
	copymem((void*)BGMap(0x1000 * bgmap)+pos+256, (void*)(vb_doom_interfaceMap+96+96), 96);
	copymem((void*)BGMap(0x1000 * bgmap)+pos+384, (void*)(vb_doom_interfaceMap+96+96+96), 96);

}