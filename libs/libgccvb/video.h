#ifndef _LIBGCCVB_VIDEO_H
#define _LIBGCCVB_VIDEO_H


#include "types.h"
#include "vip.h"


/***** Display RAM *****/
extern u32* const	L_FRAME0;
#define		CharSeg0		 0x00006000					// Characters 0-511
extern u32* const	L_FRAME1;
#define		CharSeg1		 0x0000E000					// Characters 512-1023
extern u32* const	R_FRAME0;
#define		CharSeg2		 0x00016000					// Characters 1024-1535
extern u32* const	R_FRAME1;
#define		CharSeg3		 0x0001E000					// Characters 1536-2047
#define		BGMMBase		 0x00020000					// Base address of BGMap Memory
extern u16* const	BGMM;
#define		BGMap(b)		 (BGMMBase + (b * 0x2000))	// Address of BGMap b (0 <= b <= 13)

#define		WAMBase			 0x0003D800					// Base address of World Attribute Memory
extern u16* const	WAM;
#define		World(w)		 (WAMBase + (w * 0x0020))	// Address of World w (0 <= w <= 31)
extern u16* const	CLMN_TBL;
#define		OAMBase			 0x0003E000					// Base address of Object Attribute Memory
extern u16* const	OAM;
#define		Object(o)		 (OAMBase + (o * 0x0008))	// Address of Obj o (0 <= o <= 1023)

/* Macro to set the brightness registers */
#define	SET_BRIGHT(a,b,c)       VIP_REGS[BRTA]=(u16)(a);  \
                                VIP_REGS[BRTB]=(u16)(b);  \
                                VIP_REGS[BRTC]=(u16)(c)

/* Macro to set the GPLT (BGMap palette) */
#define	SET_GPLT(n,pal)         VIP_REGS[GPLT0+n]=pal

/* Macro to set the JPLT (OBJ palette) */
#define	SET_JPLT(n,pal)         VIP_REGS[JPLT0+n]=pal

void vbWaitFrame(u16 count);
void vbDisplayOn();
void vbDisplayOff();
void vbDisplayShow();
void vbDisplayHide();
void vbFXFadeIn(u16 wait);
void vbFXFadeOut(u16 wait);

extern u8 const colTable[128];

void vbSetColTable();


#endif