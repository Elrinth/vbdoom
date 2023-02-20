#ifndef _LIBGCCVB_MEM_H
#define _LIBGCCVB_MEM_H


#include "types.h"


extern u8* const	EXPANSION;
extern u8* const	WORKRAM;
extern u16* const	SAVERAM;


/***** Ancillary Functions *****/
void copymem (u8* dest, const u8* src, u16 num);
void setmem (u8* dest, u8 src, u16 num);
void addmem (u8* dest, const u8* src, u16 num, u8 offset);


#endif