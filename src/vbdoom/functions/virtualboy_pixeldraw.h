#ifndef _FUNCTIONS_VIRTUALBOY_PIXELDRAW_H
#define _FUNCTIONS_VIRTUALBOY_PIXELDRAW_H

/*	Register for drawing control */
#define	XPEN		0x0002 	/* Start of drawing */
#define	XPENOFF		XPEN^0xffff
#define	XPRST		0x0001 	/* Forcing idling */
#define	XPRSTOFF	XPRST^0xFFFF
#define	SBOUT		0x8000 	/* In FrameBuffer drawing included */
#define	OVERTIME	0x0010 	/* Processing */
#define	XPBSYI		0x0000 	/* Idling */
#define	XPBSY0		0x0004 	/* In the midst of FrameBuffer0 picture editing */
#define	XPBSY1		0x0008 	/* In the midst of FrameBuffer1 picture editing */
#define	XPBSYR		0x000C 	/* In the midst of drawing processing reset */
/*                END NEW STUFF FROM VUCC */



#endif