#include <libgccvb.h>
#include <constants.h>
#include "virtualboy_pixeldraw.h"

void buffvln(int xcoord, int ycoord, int length, int shade, int parallax)
// Draws a vertical line in the L/R Display Buffers
{
	int pos;
	int prevpos = 0;
	int shift = (ycoord & 0x0F);
	int offset = ((xcoord << 4) + (ycoord >> 4));
	int data = 0;
	parallax <<= 4;

	for (pos = 0; pos <= length; pos++)
	{
		data |= (shade << (shift << 1));
		shift++;
		if ((shift & 0x0F) == 0)
		{
			L_FRAME0[(offset + (pos >> 4) - parallax)] |= data;
			R_FRAME0[(offset + (pos >> 4) + parallax)] |= data;
			prevpos = (pos + 16);
			shift = 0;
			data = 0;
		}
	}
	if (shift != 0)
	{
		L_FRAME0[(offset + (prevpos >> 4) - parallax)] |= data;
		R_FRAME0[(offset + (prevpos >> 4) + parallax)] |= data;
	}
}

void buffhln(int xcoord, int ycoord, int length, int shade, int parallax)
// Draws a horizontal line in the L/R Display Buffers
{
	int pos;
	int offset = ((xcoord << 4) + (ycoord >> 4));
	int data;

	for (pos = 0; pos <= (ycoord & 0x0F); pos++)
		data = (shade << (pos << 1));
	for (pos = 0; pos <= length; pos++)
	{
		L_FRAME0[((pos - parallax) << 4) + offset] |= data;
		R_FRAME0[((pos + parallax) << 4) + offset] |= data;
	}
}

void buffln(int sxcoord, int sycoord, int dxcoord, int dycoord, int shade, int parallax)
// Draws a line in the L/R Display Buffers between two points

{
	int pos;
	int ylen = (dycoord - sycoord);
	int xlen = (dxcoord - sxcoord);
	int remainder;
	int fill;
	int slide = 0;
	int slope;
	int nextpos;
	int remmult = 1;
	int lenplus;
	int temp;
	//vars for horizontal/vertical drawing embedded
	int pos1;
	int thecoord;
	int offset;
	int length;
	int prevpos;
	int shift;
	int data;

	if (((xlen + ylen) < 0) || (((xlen + ylen) == 0) && (dxcoord < sxcoord))) // flip so any coordinates work for source or destination
	{
		temp = sxcoord;
		sxcoord = dxcoord;
		dxcoord = temp;
		temp = sycoord;
		sycoord = dycoord;
		dycoord = temp;
		xlen = (dxcoord-sxcoord);
		ylen = (dycoord-sycoord);
	}

	if (xlen < 0) // actually make it the length
		xlen = ~xlen + 1;
	if (ylen < 0)
		ylen = ~ylen + 1;

	if (ylen == 0)			// prevent div by 0 and make more efficient
	{
		buffhln(sxcoord, sycoord, xlen, shade, parallax);
		return;
	}
	else if (xlen == 0)		// prevent div by 0 and make more efficient
	{
		buffvln(sxcoord, sycoord, ylen, shade, parallax);
		return;
	}

	if ((xlen / ylen) < 1)		// Y longer than X
	{
		ylen++;
		lenplus = (xlen + 1);
		slope = (ylen / lenplus);
		remainder = (ylen % lenplus);
		nextpos = sycoord;
		for (pos = 0; pos <= xlen; pos++)
		{
			if (pos == ((remmult * lenplus) / (remainder + 1)))
			{
				fill = 0;
				slide++;
				if (remmult < remainder)
					remmult++;
			}
			else
				fill = -1;

			if ((dxcoord-sxcoord) < 0)
			{
				prevpos = 0;
				shift = (nextpos & 0x0F);
				data = 0;
				length = (slope + fill);
				thecoord = (sxcoord - pos);
				offset = ((thecoord << 4) + (nextpos >> 4));
				parallax <<= 4;

				for (pos1 = 0; pos1 <= length; pos1++)
				{
					data |= (shade << (shift << 1));
					shift++;
					if ((shift & 0x0F) == 0)
					{
						L_FRAME0[(offset + (pos1 >> 4) - parallax)] |= data;
						R_FRAME0[(offset + (pos1 >> 4) + parallax)] |= data;
						prevpos = (pos1 + 16);
						shift = 0;
						data = 0;
					}
				}
				if (shift != 0)
				{
					L_FRAME0[(offset + (prevpos >> 4) - parallax)] |= data;
					R_FRAME0[(offset + (prevpos >> 4) + parallax)] |= data;
				}
			}
			else
			{
				prevpos = 0;
				shift = (nextpos & 0x0F);
				data = 0;
				length = (slope + fill);
				thecoord = (sxcoord + pos);
				offset = ((thecoord << 4) + (nextpos >> 4));
				parallax <<= 4;

				for (pos1 = 0; pos1 <= length; pos1++)
				{
					data |= (shade << (shift << 1));
					shift++;
					if ((shift & 0x0F) == 0)
					{
						L_FRAME0[(offset + (pos1 >> 4) - parallax)] |= data;
						R_FRAME0[(offset + (pos1 >> 4) + parallax)] |= data;
						prevpos = (pos1 + 16);
						shift = 0;
						data = 0;
					}
				}
				if (shift != 0)
				{
					L_FRAME0[(offset + (prevpos >> 4) - parallax)] |= data;
					R_FRAME0[(offset + (prevpos >> 4) + parallax)] |= data;
				}
			}
			nextpos = (sycoord + (pos + 1) * slope + slide);
		}
	}

	else			// X longer or same as Y
	{
		xlen++;
		lenplus = (ylen + 1);
		slope = (xlen / lenplus);
		remainder = (xlen % lenplus);
		nextpos = sxcoord;
		for (pos = 0; pos <= ylen; pos++)
		{
			if (pos == ((remmult * lenplus) / (remainder + 1)))
			{
				fill = 0;
				slide++;
				if (remmult < remainder)
					remmult++;
			}
			else
				fill = -1;

			if ((dycoord-sycoord) < 0)
			{
				thecoord = (sycoord - pos);
				offset = ((nextpos << 4) + (thecoord >> 4));
				length = (slope + fill);

				for (pos1 = 0; pos1 <= (thecoord & 0x0F); pos1++)
					data = (shade << (pos1 << 1));
				for (pos1 = 0; pos1 <= length; pos1++)
				{
					L_FRAME0[((pos1 - parallax) << 4) + offset] |= data;
					R_FRAME0[((pos1 + parallax) << 4) + offset] |= data;
				}
			}
			else
			{
				thecoord = (sycoord + pos);
				offset = ((nextpos << 4) + (thecoord >> 4));
				length = (slope + fill);

				for (pos1 = 0; pos1 <= (thecoord & 0x0F); pos1++)
					data = (shade << (pos1 << 1));
				for (pos1 = 0; pos1 <= length; pos1++)
				{
					L_FRAME0[((pos1 - parallax) << 4) + offset] |= data;
					R_FRAME0[((pos1 + parallax) << 4) + offset] |= data;
				}
			}
			nextpos = (sxcoord + (pos + 1) * slope + slide);
		}
	}
}

int readpixel(int xcoord, int ycoord, int parallax)
// Reads the data of a pixel
{
	int pos;
	int offset = ((xcoord << 4) + (ycoord >> 4));
	int data;

	data = L_FRAME0[((0 - parallax) << 4) + offset];
	data &= R_FRAME0[(parallax << 4) + offset];

	for (pos = 0; pos != (ycoord & 0x0F); pos++); // gets pos to the right position

	return ((data >> (pos << 1)) & 3); // shifts to first 2 bits and masks
}