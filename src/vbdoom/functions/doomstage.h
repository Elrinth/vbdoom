#ifndef _FUNCTIONS_DOOMSTAGE_H
#define _FUNCTIONS_DOOMSTAGE_H


#include <types.h>
#include <math.h>

// Utility functions. Because C doesn't have templates,
// we use the slightly less safe preprocessor macros to
// implement these functions that work with multiple types.
#define min(a,b)             (((a) < (b)) ? (a) : (b)) // min: Choose smaller of two scalars.
#define max(a,b)             (((a) > (b)) ? (a) : (b)) // max: Choose greater of two scalars.
#define clamp(a, mi,ma)      min(max(a,mi),ma)         // clamp: Clamp value into set range.
#define vxs(x0,y0, x1,y1)    ((x0)*(y1) - (x1)*(y0))   // vxs: Vector cross product
#define fvxs(x0,y0, x1,y1)    (FIX13_3_MULT(x0, y1) - FIX13_3_MULT(x1, y0))   // fvxs: Vector cross product
// Overlap:  Determine whether the two number ranges overlap.
#define Overlap(a0,a1,b0,b1) (min(a0,a1) <= max(b0,b1) && min(b0,b1) <= max(a0,a1))
// IntersectBox: Determine whether two 2D-boxes intersect.
#define IntersectBox(x0,y0, x1,y1, x2,y2, x3,y3) (Overlap(x0,x1,x2,x3) && Overlap(y0,y1,y2,y3))
// PointSide: Determine which side of a line the point is on. Return value: <0, =0 or >0.
#define PointSide(px,py, x0,y0, x1,y1) vxs((x1)-(x0), (y1)-(y0), (px)-(x0), (py)-(y0))
// Intersect: Calculate the point of intersection between two lines.
#define Intersect(x1,y1, x2,y2, x3,y3, x4,y4) ((struct xy) { \
				vxs(vxs(x1,y1, x2,y2), (x1)-(x2), vxs(x3,y3, x4,y4), (x3)-(x4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)), \
				vxs(vxs(x1,y1, x2,y2), (y1)-(y2), vxs(x3,y3, x4,y4), (y3)-(y4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)) })


// FixedPointSide: Determine which side of a line the point is on. Return value: <0, =0 or >0.
#define FixedPointSide(px,py, x0,y0, x1,y1) vxs((x1)-(x0), (y1)-(y0), (px)-(x0), (py)-(y0))

#define FixedIntersect(x1,y1, x2,y2, x3,y3, x4,y4) ((struct xy) { \
	FIX13_3_DIV( fvxs(fvxs(x1,y1, x2,y2), (x1)-(x2), fvxs(x3,y3, x4,y4), (x3)-(x4)), fvxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)) ), \
	FIX13_3_DIV( fvxs(fvxs(x1,y1, x2,y2), (y1)-(y2), fvxs(x3,y3, x4,y4), (y3)-(y4)), fvxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)) ) })


void initializeDoomStage();

void fixedDrawDoomStage(u8 bgmap);
void drawDoomStage(u8 bgmap);

void drawDoomStageTile(int iX, int iStartY, int iEndY, u16 iStartPos);
void incPlayerAngle(float iValue);
void jawPlayer(float iValue);
void playerMoveForward(float iValue);
void playerStrafe(float iValue);

void incFixedPlayerAngle(s32 iValue);
void jawFixedPlayer(s32 iValue);
void fixedPlayerMoveForward(s32 iValue);
void fixedPlayerStrafe(s32 iValue);

void affine_fast_scale_fixed(u8 world, s32 scale);

#endif