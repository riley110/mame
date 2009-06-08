/***************************************************************************
  TI-85 driver by Krzysztof Strzecha

  Functions to emulate the video hardware of the TI-85

***************************************************************************/

#include "driver.h"
#include "includes/ti85.h"

#define TI81_VIDEO_MEMORY_SIZE	 768
#define TI81_SCREEN_X_SIZE	  12
#define TI81_SCREEN_Y_SIZE	  64
#define TI81_NUMBER_OF_FRAMES	   6

#define TI82_VIDEO_MEMORY_SIZE	 768
#define TI82_SCREEN_X_SIZE	  12
#define TI82_SCREEN_Y_SIZE	  64
#define TI82_NUMBER_OF_FRAMES	   6

#define TI85_VIDEO_MEMORY_SIZE	1024
#define TI85_SCREEN_X_SIZE	  16
#define TI85_SCREEN_Y_SIZE	  64
#define TI85_NUMBER_OF_FRAMES	   6

#define TI86_VIDEO_MEMORY_SIZE	1024
#define TI86_SCREEN_X_SIZE	  16
#define TI86_SCREEN_Y_SIZE	  64
#define TI86_NUMBER_OF_FRAMES	   6

static int ti_video_memory_size;
static int ti_screen_x_size;
static int ti_screen_y_size;
static int ti_number_of_frames;

static UINT8 * ti85_frames;

static const unsigned char ti85_colors[32*7][3] =
{
	{ 0xae, 0xcd, 0xb0 },  	{ 0xaa, 0xc9, 0xae },  	{ 0xa6, 0xc5, 0xad },  	{ 0xa3, 0xc1, 0xab },  	{ 0x9f, 0xbd, 0xaa },  	{ 0x9b, 0xb9, 0xa8 },  	{ 0x98, 0xb5, 0xa7 },  //0x00
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa9, 0xc8, 0xae },  	{ 0xa4, 0xc3, 0xac },  	{ 0xa0, 0xbe, 0xaa },  	{ 0x9b, 0xb9, 0xa8 },  	{ 0x96, 0xb4, 0xa6 },  	{ 0x92, 0xaf, 0xa4 },  //0x01
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa8, 0xc7, 0xad },  	{ 0xa2, 0xc1, 0xab },  	{ 0x9d, 0xbb, 0xa9 },  	{ 0x97, 0xb5, 0xa6 },  	{ 0x91, 0xaf, 0xa4 },  	{ 0x8c, 0xa9, 0xa2 },  //0x02
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa7, 0xc6, 0xad },  	{ 0xa0, 0xbf, 0xaa },  	{ 0x9a, 0xb8, 0xa7 },  	{ 0x93, 0xb1, 0xa4 },  	{ 0x8c, 0xaa, 0xa1 },  	{ 0x86, 0xa3, 0x9f },  //0x03
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa6, 0xc5, 0xac },  	{ 0x9f, 0xbd, 0xa9 },  	{ 0x97, 0xb5, 0xa6 },  	{ 0x90, 0xad, 0xa3 },  	{ 0x88, 0xa5, 0xa0 },  	{ 0x81, 0x9d, 0x9d },  //0x04
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa5, 0xc4, 0xac },  	{ 0x9d, 0xbb, 0xa8 },  	{ 0x94, 0xb2, 0xa5 },  	{ 0x8c, 0xa9, 0xa1 },  	{ 0x83, 0xa0, 0x9d },  	{ 0x7b, 0x97, 0x9a },  //0x05
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa4, 0xc3, 0xac },  	{ 0x9b, 0xb9, 0xa8 },  	{ 0x91, 0xaf, 0xa4 },  	{ 0x88, 0xa5, 0xa0 },  	{ 0x7e, 0x9b, 0x9c },  	{ 0x75, 0x92, 0x98 },  //0x06
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa3, 0xc2, 0xab },  	{ 0x99, 0xb7, 0xa7 },  	{ 0x8e, 0xac, 0xa3 },  	{ 0x84, 0xa1, 0x9e },  	{ 0x79, 0x96, 0x9a },  	{ 0x6f, 0x8c, 0x96 },  //0x07
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa2, 0xc1, 0xab },  	{ 0x97, 0xb5, 0xa6 },  	{ 0x8c, 0xa9, 0xa1 },  	{ 0x80, 0x9d, 0x9c },  	{ 0x75, 0x91, 0x97 },  	{ 0x6a, 0x86, 0x93 },  //0x08
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa1, 0xc0, 0xaa },  	{ 0x95, 0xb3, 0xa5 },  	{ 0x89, 0xa6, 0xa0 },  	{ 0x7c, 0x99, 0x9b },  	{ 0x70, 0x8c, 0x96 },  	{ 0x64, 0x80, 0x91 },  //0x09
	{ 0xae, 0xcd, 0xb0 },  	{ 0xa0, 0xbf, 0xaa },  	{ 0x93, 0xb1, 0xa4 },  	{ 0x86, 0xa3, 0x9f },  	{ 0x78, 0x95, 0x99 },  	{ 0x6b, 0x87, 0x93 },  	{ 0x5e, 0x7a, 0x8e },  //0x0a
	{ 0xae, 0xcd, 0xb0 },  	{ 0x9f, 0xbe, 0xaa },  	{ 0x91, 0xaf, 0xa4 },  	{ 0x83, 0xa0, 0x9e },  	{ 0x74, 0x91, 0x98 },  	{ 0x66, 0x82, 0x92 },  	{ 0x58, 0x74, 0x8c },  //0x0b
	{ 0xae, 0xcd, 0xb0 },  	{ 0x9e, 0xbd, 0xa9 },  	{ 0x8f, 0xad, 0xa3 },  	{ 0x80, 0x9e, 0x9d },  	{ 0x71, 0x8e, 0x96 },  	{ 0x62, 0x7e, 0x90 },  	{ 0x53, 0x6f, 0x8a },  //0x0c
	{ 0xa9, 0xc8, 0xae },  	{ 0x9a, 0xb9, 0xa8 },  	{ 0x8c, 0xaa, 0xa2 },  	{ 0x7e, 0x9b, 0x9c },  	{ 0x6f, 0x8c, 0x96 },  	{ 0x61, 0x7d, 0x90 },  	{ 0x53, 0x6f, 0x8a },  //0x0d
	{ 0xa4, 0xc3, 0xac },  	{ 0x96, 0xb5, 0xa6 },  	{ 0x89, 0xa7, 0xa0 },  	{ 0x7b, 0x99, 0x9b },  	{ 0x6e, 0x8b, 0x95 },  	{ 0x60, 0x7d, 0x8f },  	{ 0x53, 0x6f, 0x8a },  //0x0e
	{ 0xa0, 0xbe, 0xaa },  	{ 0x93, 0xb0, 0xa4 },  	{ 0x86, 0xa3, 0x9f },  	{ 0x79, 0x96, 0x9a },  	{ 0x6c, 0x89, 0x94 },  	{ 0x5f, 0x7c, 0x8f },  	{ 0x53, 0x6f, 0x8a },  //0x0f
	{ 0x9b, 0xba, 0xa8 },  	{ 0x8f, 0xad, 0xa3 },  	{ 0x83, 0xa1, 0x9e },  	{ 0x77, 0x94, 0x99 },  	{ 0x6b, 0x88, 0x94 },  	{ 0x5f, 0x7b, 0x8f },  	{ 0x53, 0x6f, 0x8a },  //0x10
	{ 0x97, 0xb5, 0xa6 },  	{ 0x8b, 0xa9, 0xa1 },  	{ 0x80, 0x9d, 0x9c },  	{ 0x75, 0x92, 0x98 },  	{ 0x69, 0x86, 0x93 },  	{ 0x5e, 0x7a, 0x8e },  	{ 0x53, 0x6f, 0x8a },  //0x11
	{ 0x92, 0xb0, 0xa4 },  	{ 0x87, 0xa5, 0x9f },  	{ 0x7d, 0x9a, 0x9b },  	{ 0x72, 0x8f, 0x97 },  	{ 0x68, 0x84, 0x92 },  	{ 0x5d, 0x79, 0x8e },  	{ 0x53, 0x6f, 0x8a },  //0x12
	{ 0x8e, 0xac, 0xa2 },  	{ 0x84, 0xa1, 0x9e },  	{ 0x7a, 0x97, 0x9a },  	{ 0x70, 0x8d, 0x96 },  	{ 0x66, 0x83, 0x92 },  	{ 0x5c, 0x79, 0x8e },  	{ 0x53, 0x6f, 0x8a },  //0x13
	{ 0x89, 0xa7, 0xa0 },  	{ 0x80, 0x9d, 0x9c },  	{ 0x77, 0x94, 0x98 },  	{ 0x6e, 0x8b, 0x95 },  	{ 0x65, 0x81, 0x91 },  	{ 0x5c, 0x78, 0x8d },  	{ 0x53, 0x6f, 0x8a },  //0x14
	{ 0x85, 0xa2, 0x9e },  	{ 0x7c, 0x99, 0x9a },  	{ 0x74, 0x91, 0x97 },  	{ 0x6c, 0x88, 0x94 },  	{ 0x63, 0x80, 0x90 },  	{ 0x5b, 0x77, 0x8d },  	{ 0x53, 0x6f, 0x8a },  //0x15
	{ 0x80, 0x9e, 0x9d },  	{ 0x78, 0x96, 0x99 },  	{ 0x71, 0x8e, 0x96 },  	{ 0x69, 0x86, 0x93 },  	{ 0x62, 0x7e, 0x90 },  	{ 0x5a, 0x76, 0x8d },  	{ 0x53, 0x6f, 0x8a },  //0x16
	{ 0x7c, 0x99, 0x9b },  	{ 0x75, 0x92, 0x98 },  	{ 0x6e, 0x8b, 0x95 },  	{ 0x67, 0x84, 0x92 },  	{ 0x60, 0x7d, 0x8f },  	{ 0x59, 0x76, 0x8c },  	{ 0x53, 0x6f, 0x8a },  //0x17
	{ 0x77, 0x94, 0x99 },  	{ 0x71, 0x8d, 0x96 },  	{ 0x6b, 0x87, 0x94 },  	{ 0x65, 0x81, 0x91 },  	{ 0x5f, 0x7b, 0x8f },  	{ 0x59, 0x75, 0x8c },  	{ 0x53, 0x6f, 0x8a },  //0x18
	{ 0x73, 0x90, 0x97 },  	{ 0x6d, 0x8a, 0x94 },  	{ 0x68, 0x85, 0x92 },  	{ 0x63, 0x7f, 0x90 },  	{ 0x5d, 0x7a, 0x8e },  	{ 0x58, 0x74, 0x8c },  	{ 0x53, 0x6f, 0x8a },  //0x19
	{ 0x6e, 0x8b, 0x95 },  	{ 0x69, 0x86, 0x93 },  	{ 0x65, 0x81, 0x91 },  	{ 0x60, 0x7d, 0x8f },  	{ 0x5c, 0x78, 0x8d },  	{ 0x57, 0x73, 0x8b },  	{ 0x53, 0x6f, 0x8a },  //0x1a
	{ 0x6a, 0x86, 0x93 },  	{ 0x66, 0x82, 0x91 },  	{ 0x62, 0x7e, 0x90 },  	{ 0x5e, 0x7a, 0x8e },  	{ 0x5a, 0x76, 0x8d },  	{ 0x56, 0x72, 0x8b },  	{ 0x53, 0x6f, 0x8a },  //0x1b
	{ 0x65, 0x82, 0x91 },  	{ 0x62, 0x7e, 0x8f },  	{ 0x5f, 0x7b, 0x8e },  	{ 0x5c, 0x78, 0x8d },  	{ 0x59, 0x75, 0x8c },  	{ 0x56, 0x72, 0x8b },  	{ 0x53, 0x6f, 0x8a },  //0x1c
	{ 0x61, 0x7d, 0x8f },  	{ 0x5e, 0x7a, 0x8e },  	{ 0x5c, 0x78, 0x8d },  	{ 0x5a, 0x76, 0x8c },  	{ 0x57, 0x73, 0x8b },  	{ 0x55, 0x71, 0x8a },  	{ 0x53, 0x6f, 0x8a },  //0x1d
	{ 0x5c, 0x78, 0x8d },  	{ 0x5a, 0x76, 0x8c },  	{ 0x59, 0x75, 0x8c },  	{ 0x57, 0x73, 0x8b },  	{ 0x56, 0x72, 0x8b },  	{ 0x54, 0x70, 0x8a },  	{ 0x53, 0x6f, 0x8a },  //0x1e
	{ 0x57, 0x74, 0x8c },  	{ 0x56, 0x73, 0x8b },  	{ 0x55, 0x72, 0x8b },  	{ 0x55, 0x71, 0x8b },  	{ 0x54, 0x70, 0x8a },  	{ 0x53, 0x6f, 0x8a },  	{ 0x53, 0x6f, 0x8a }  //0x1f
};

static const unsigned short ti85_palette[32][7] =
{
	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 },
	{ 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d },
	{ 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14 },
	{ 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b },
	{ 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22 },
	{ 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29 },
	{ 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30 },
	{ 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 },
	{ 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e },
	{ 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45 },
	{ 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c },
	{ 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53 },
	{ 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a },
	{ 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61 },
	{ 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68 },
	{ 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f },
	{ 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76 },
	{ 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d },
	{ 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84 },
	{ 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b },
	{ 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92 },
	{ 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99 },
	{ 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0 },
	{ 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7 },
	{ 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae },
	{ 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5 },
	{ 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc },
	{ 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3 },
	{ 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca },
	{ 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1 },
	{ 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8 },
	{ 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf }
};

PALETTE_INIT( ti85 )
{
	UINT8 i, j, r, g, b;

	machine->colortable = colortable_alloc(machine, 224);

	for ( i = 0; i < 224; i++ )
	{
		r = ti85_colors[i][0]; g = ti85_colors[i][1]; b = ti85_colors[i][2];
		colortable_palette_set_color(machine->colortable, i, MAKE_RGB(r, g, b));
	}

	for (i=0; i < 32; i++)
		for (j=0; j < 7; j++)
			colortable_entry_set_value(machine->colortable, i*7+j, ti85_palette[i][j]);

	if (!strncmp(machine->gamedrv->name, "ti81", 4))
	{
		ti_video_memory_size = TI81_VIDEO_MEMORY_SIZE;
		ti_screen_x_size = TI81_SCREEN_X_SIZE;
		ti_screen_y_size = TI81_SCREEN_Y_SIZE;
		ti_number_of_frames = TI81_NUMBER_OF_FRAMES;
	}
	else if (!strncmp(machine->gamedrv->name, "ti85", 4))
	{
		ti_video_memory_size = TI85_VIDEO_MEMORY_SIZE;
		ti_screen_x_size = TI85_SCREEN_X_SIZE;
		ti_screen_y_size = TI85_SCREEN_Y_SIZE;
		ti_number_of_frames = TI85_NUMBER_OF_FRAMES;
	}
	else if (!strncmp(machine->gamedrv->name, "ti82", 4))
	{
		ti_video_memory_size = TI82_VIDEO_MEMORY_SIZE;
		ti_screen_x_size = TI82_SCREEN_X_SIZE;
		ti_screen_y_size = TI82_SCREEN_Y_SIZE;
		ti_number_of_frames = TI82_NUMBER_OF_FRAMES;
	}
	else if (!strncmp(machine->gamedrv->name, "ti86", 4))
	{
		ti_video_memory_size = TI86_VIDEO_MEMORY_SIZE;
		ti_screen_x_size = TI86_SCREEN_X_SIZE;
		ti_screen_y_size = TI86_SCREEN_Y_SIZE;
		ti_number_of_frames = TI86_NUMBER_OF_FRAMES;
	}
	else
	{
		/* don't allocate memory for the others drivers */
		return;
	}

	ti85_frames = auto_alloc_array_clear(machine, UINT8, ti_number_of_frames*ti_video_memory_size);
}

VIDEO_START( ti85 )
{
}

VIDEO_UPDATE( ti85 )
{
	const address_space *space = cputag_get_address_space(screen->machine, "maincpu", ADDRESS_SPACE_PROGRAM);
	int x,y,b;
	int brightnes;
	int lcdmem;

	if (!ti85_LCD_status || !ti85_timer_interrupt_mask)
	{
		for (y=0; y<ti_screen_y_size; y++)
			for (x=0; x<ti_screen_x_size; x++)
				for (b=0; b<8; b++)
					*BITMAP_ADDR16(bitmap, y, x*8+b) = ti85_palette[ti85_LCD_contrast&0x1f][6];
		return 0;
	}

	lcdmem =  ((ti85_LCD_memory_base & 0x3F) + 0xc0) << 0x08;

	memcpy (ti85_frames, ti85_frames+ti_video_memory_size, sizeof (UINT8) * (ti_number_of_frames-1) * ti_video_memory_size);

        for (y=0; y<ti_screen_y_size; y++)
		for (x=0; x<ti_screen_x_size; x++)
			*(ti85_frames+(ti_number_of_frames-1)*ti_video_memory_size+y*ti_screen_x_size+x) = memory_read_byte(space, lcdmem+y*ti_screen_x_size+x);

       	for (y=0; y<ti_screen_y_size; y++)
		for (x=0; x<ti_screen_x_size; x++)
			for (b=0; b<8; b++)
			{
				brightnes = ((*(ti85_frames+0*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+1*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+2*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+3*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+4*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+5*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01);

				*BITMAP_ADDR16(bitmap, y, x*8+b) = ti85_palette[ti85_LCD_contrast&0x1f][brightnes];
	                }
	return 0;
}

VIDEO_UPDATE( ti82 )
{
	//for now use the ti85_palette

	int x,y,b;
	int brightnes;

	if (!ti85_LCD_status || !ti85_timer_interrupt_mask)
	{
		for (y=0; y<ti_screen_y_size; y++)
			for (x=0; x<ti_screen_x_size; x++)
				for (b=0; b<8; b++)
					*BITMAP_ADDR16(bitmap, y, x*8+b) = ti85_palette[ti85_LCD_contrast&0x1f][6];
		return 0;
	}

	memcpy (ti85_frames, ti85_frames+ti_video_memory_size, sizeof (UINT8) * (ti_number_of_frames-1) * ti_video_memory_size);

        for (y=0; y<ti_screen_y_size; y++)
		for (x=0; x<ti_screen_x_size; x++)
			*(ti85_frames+(ti_number_of_frames-1)*ti_video_memory_size+y*ti_screen_x_size+x) = ti82_video_buffer[ y*ti_screen_x_size+x];

       	for (y=0; y<ti_screen_y_size; y++)
		for (x=0; x<ti_screen_x_size; x++)
			for (b=0; b<8; b++)
			{
				brightnes = ((*(ti85_frames+0*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+1*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+2*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+3*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+4*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01)
					  + ((*(ti85_frames+5*ti_video_memory_size+y*ti_screen_x_size+x)>>(7-b)) & 0x01);

				*BITMAP_ADDR16(bitmap, y, x*8+b) = ti85_palette[ti85_LCD_contrast&0x1f][brightnes];
	                }
	return 0;
}

