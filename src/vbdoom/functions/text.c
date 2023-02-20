#include <libgccvb.h>
#include <constants.h>
#include <languages.h>
#include "text.h"

extern u8 currentLanguage;

void printString(u8 bgmap, u16 x, u16 y, char* t_string)
{
	// font consists of the last 256 chars (1792-2047)
	u16 i=0, pos=0, col=x;

	while(t_string[i]) {
		pos = (y << 6) + x;

		switch(t_string[i]) {

			case 9:
				// Tab
				x = (((x << 2) + 1) >> 2);
				break;

			case 10:
				// Carriage Return
				y++;
				x = col;
				break;

			case 13:
				x = col;
				break;

			default:
				BGMM[(0x1000 * bgmap) + pos] = (u8)t_string[i] + 0x700;
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

char* getString(int string)
{
	return (char*)__LANGUAGES[currentLanguage][string];
}