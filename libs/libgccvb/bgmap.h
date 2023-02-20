#ifndef _LIBGCCVB_BGMAP_H
#define _LIBGCCVB_BGMAP_H


#include "types.h"
#include "video.h"


/***** BGMap Macros (For the lazy ;) *****/
/*
	These calculate the address of a BGMap cell from a segment #
	and X, Y coordinates.  (Which range from 0 to 63)  Not very
	efficient for games, but useful for pre-calculation, debugging,
	or easier-to-read example code, I suppose...
*/

// Procedural
/***************************/

// Set a cell's char #.
#define BGM_CSET(n,x,y,c)		BGMM[((n) << 12) + ((y) << 6) + (x)] = (BGMM[((n) << 12) + ((y) << 6) + (x)] & 0xF000) | (u16)(c)
// Turns on horizontal flipping for a cell.
#define BGM_HSET(n,x,y)			BGMM[((n) << 12) + ((y) << 6) + (x)] |= BGM_HFLIP
// Turns off horizontal flipping for a cell.
#define BGM_HCLEAR(n,x,y)		BGMM[((n) << 12) + ((y) << 6) + (x)] &= 0xDFFF
// Turns on vertical flipping for a cell.
#define BGM_VSET(n,x,y)			BGMM[((n) << 12) + ((y) << 6) + (x)] |= BGM_VFLIP
// Turns off vertical flipping for a cell.
#define BGM_VCLEAR(n,x,y)		BGMM[((n) << 12) + ((y) << 6) + (x)] &= 0xEFFF
// Set the palette used by a cell.  Example: BGM_PALSET(0,1,2,BGM_PAL3);
#define BGM_PALSET(n,x,y,p)		BGMM[((n) << 12) + ((y) << 6) + (x)] = (BGMM[((n) << 12) + ((y) << 6) + (x)] & 0x3FFF) | (u16)(p)


// Expressional
/***************************/

// Return a cell's char #.
#define	BGM_CGET(n,x,y)			(BGMM[((n) << 12) + ((y) << 6) + (x)] & (u16)0x7FF)
// Return -1 if a cell is flipped horizontally; 0 if not.
#define BGM_HGET(n,x,y)			((BGMM[((n) << 12) + ((y) << 6) + (x)] & (u16)BGM_HFLIP) == BGM_HFLIP ? -1 : 0)
// Return -1 if a cell is flipped vertically; 0 if not.
#define BGM_VGET(n,x,y)			((BGMM[((n) << 12) + ((y) << 6) + (x)] & (u16)BGM_VFLIP) == BGM_VFLIP ? -1 : 0)
// Return a cell's palette.  Example: BGM_PALGET(BGM_PAL2);
#define BGM_PALGET(n,x,y)		(BGMM[((n) << 12) + ((y) << 6) + (x)] & (u16)0xC000)

/* BGMap Flags */
/* OR these together with a Char # to build a BGMap Cell
		-OR-
   Compare the output of the BGM_?GET macros above.
*/

#define	BGM_PAL0	0x0000
#define	BGM_PAL1	0x4000
#define	BGM_PAL2	0x8000
#define	BGM_PAL3	0xC000

#define	BGM_HFLIP	0x2000
#define	BGM_VFLIP	0x1000


#endif
