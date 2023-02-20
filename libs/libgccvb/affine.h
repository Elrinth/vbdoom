// affine.h
// v0.1 beta (preliminary scaling functions added)
// functions and macros to make affine transformations easier
// for use with gccVB
// written by Parasyte (parasytic_i[at]yahoo.com)

#ifndef _LIBGCCVB_AFFINE_H
#define _LIBGCCVB_AFFINE_H


#include "types.h"
#include "video.h"


#define fixed_7_9(n)		(f32)(n * (1<<9))			//convert from float\int\etc to 7.9 fixed
#define fixed_13_3(n)		(f16)(n * (1<<3))			//convert from float\int\etc to 13.3 fixed
#define inverse_fixed(n)	(f16)((1<<18)/fixed_7_9(n))	//convert from float\int\etc to 7.9 fixed (with inversion)


void affine_clr_param(u8 world);
void affine_scale(u8 world, s16 centerX, s16 centerY, u16 imageW, u16 imageH, float scaleX, float scaleY);
void affine_fast_scale(u8 world, float scale);


#endif