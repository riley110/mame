/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *snowbros_paletteram;
unsigned char *snowbros_spriteram;

int snowbros_spriteram_size;


void snowbros_paletteram_w (int offset, int data)
{
	int oldword = READ_WORD(&snowbros_paletteram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	int r,g,b;


	WRITE_WORD(&snowbros_paletteram[offset],newword);

	r = newword & 31;
	g = (newword >> 5) & 31;
	b = (newword >> 10) & 31;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	palette_change_color(offset / 2,r,g,b);
}

int  snowbros_paletteram_r (int offset)
{
	return READ_WORD(&snowbros_paletteram[offset]);
}


/* Put in case screen can be optimised later */

void snowbros_spriteram_w (int offset, int data)
{
  	COMBINE_WORD_MEM(&snowbros_spriteram[offset], data);
}

int  snowbros_spriteram_r (int offset)
{
	return READ_WORD(&snowbros_spriteram[offset]);
}

void snowbros_vh_screenrefresh(struct osd_bitmap *bitmap)
{
    int x=0,y=0,offs;


	/* recalc the palette if necessary */
	palette_recalc ();


    /*
     * Sprite Tile Format
     * ------------------
     *
     * Byte(s) | Bit(s)   | Use
     * --------+-76543210-+----------------
     *  0-5    | -------- | ?
     *    6    | -------- | ?
     *    7    | xxxx.... | Palette Bank
     *    7    | .......x | XPos - Sign Bit
     *    9    | xxxxxxxx | XPos
     *    7    | ......x. | YPos - Sign Bit
     *    B    | xxxxxxxx | YPos
     *    7    | .....x.. | Use Relative offsets
     *    C    | -------- | ?
     *    D    | xxxxxxxx | Sprite Number (low 8 bits)
     *    E    | -------- | ?
     *    F    | ....xxxx | Sprite Number (high 4 bits)
     *    F    | x....... | Flip Sprite Y-Axis
     *    F    | .x...... | Flip Sprite X-Axis
     */

	/* This clears & redraws the entire screen each pass */

  	fillbitmap(bitmap,Machine->gfx[0]->colortable[0],&Machine->drv->visible_area);

	for (offs = 0;offs < 0x1e00; offs += 16)
	{
		int sx = READ_WORD(&snowbros_spriteram[8+offs]) & 0xff;
		int sy = READ_WORD(&snowbros_spriteram[0x0a+offs]) & 0xff;
    	int tilecolour = READ_WORD(&snowbros_spriteram[6+offs]);

        if (tilecolour & 1) sx = -1 - (sx ^ 0xff);

        if (tilecolour & 2) sy = -1 - (sy ^ 0xff);

        if (tilecolour & 4)
        {
        	x += sx;
            y += sy;
        }
        else
        {
        	x = sx;
            y = sy;
        }

        if (x > 511) x &= 0x1ff;
        if (y > 511) y &= 0x1ff;

        if ((x>-16) && (y>0) && (x<256) && (y<240))
        {
            int flip = READ_WORD(&snowbros_spriteram[0x0e + offs]);
        	int tile = ((flip & 0x0f) << 8) + (READ_WORD(&snowbros_spriteram[0x0c+offs]) & 0xff);

			drawgfx(bitmap,Machine->gfx[0],
				tile,
				(tilecolour & 0xf0) >> 4,
				flip & 0x80, flip & 0x40,
				x,y,
				&Machine->drv->visible_area,TRANSPARENCY_PEN,0);
		}
	}
}
