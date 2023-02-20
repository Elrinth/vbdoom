#include <libgccvb.h>
#include <mem.h>
#include <video.h>
#include <world.h>
#include <constants.h>
#include <languages.h>
#include "doomguy.h"

extern BYTE vb_doom_guy_facesTiles[];
extern BYTE vb_doom_guy_facesMap[];


//Total size: 384 + 72 = 456

void drawDoomGuy(u8 bgmap, u16 x, u16 y, u8 face)
{
	//copymem((void*)CharSeg0, (void*)ffTiles, 256);
	//copymem((void*)BGMap(0), (void*)ffMapvb, 10);

	//copymem((void*)(CharSeg0), (void*)ffTiles, 256);
	//copymem((void*)BGMap(0), (void*)ffMap, 40);
	u16 pos=0;
	pos = (y << 7) + x;

	u16 whichFace = (face*24);
	copymem((void*)(CharSeg0+2352), (void*)vb_doom_guy_facesTiles, 384);

	copymem((void*)BGMap(0x1000 * bgmap)+pos, (void*)vb_doom_guy_facesMap+whichFace, 6);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+128), (void*)(vb_doom_guy_facesMap+6+whichFace), 6);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+256), (void*)(vb_doom_guy_facesMap+12+whichFace), 6);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+384), (void*)(vb_doom_guy_facesMap+18+whichFace), 6);

/*
	copymem((void*)BGMap(0x1000 * bgmap)+pos, 0, 8);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+128), 0+8, 8);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+256), 0+16, 8);
	copymem((void*)(BGMap(0x1000 * bgmap)+pos+384), 0+24, 8);*/


	// font consists of the last 256 chars (1792-2047)


}