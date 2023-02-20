#include "types.h"
#include "mem.h"
#include "video.h"
#include "misc.h"


const char nums[16]="0123456789ABCDEF";

char *itoa(u32 num, u8 base, u8 digits) 
{
	int i;
	static char rev[11];

	for (i = 0; i < 10; i++) {
		rev[9-i] = nums[num%base];
		num /= base;
	}

	i=0;
	while (rev[i] == '0') i++;
	if (i >= (10-digits)) i=(10-digits);

	rev[10] = 0;

	return rev+i;
}

void cls() 
{
	setmem((void*)(BGMap(0)), 0, 8192);
}

void vbTextOut(u16 bgmap, u16 col, u16 row, char *t_string)
/* The font must reside in Character segment 3 */
{
	u16 i = 0,
	pos = row * 64 + col;

	while(t_string[i])
	{
		//BGMM[(0x1000 * bgmap) + pos + i] = (u16)t_string[i] - 32 + 0x600;
		BGMM[(0x1000 * bgmap) + pos + i] = (u16)t_string[i] + 0x600;
		i++;
	}
}

void vbPrint(u8 bgmap, u16 x, u16 y, char *t_string, u16 bplt)
{
// Font consists of the last 256 chars (1792-2047)
	u16 i=0,pos=0,col=x;

	while(t_string[i])
	{
		pos = (y << 6) + x;

		switch(t_string[i])
		{
			case 7:
				// Bell (!)
				break;
			case 9:
				// Horizontal Tab
				x = (x / tabsize + 1) * tabsize;
				break;
			case 10:
				// Carriage Return
				y++;
				x = col;
				break;
			case 13:
				// Line Feed
				// x = col;
				break;
			default:
				BGMM[(0x1000 * bgmap) + pos] = ((u16)t_string[i] + 0x700) | (bplt << 14);
				if (x++ > 63)
				{
					x = col;
					y++;
				}
				break;
		}
		i++;
	}
}