#ifndef _LIBGCCVB_MISC_H
#define _LIBGCCVB_MISC_H


#include "types.h"
#include "mem.h"
#include "video.h"

// horizontal tab size in chars
#define tabsize 4

char *itoa(u32 num, u8 base, u8 digits);
void cls();
void vbTextOut(u16 bgmap, u16 col, u16 row, char *t_string);
void vbPrint(u8 bgmap, u16 x, u16 y, char *t_string, u16 bplt);


#endif