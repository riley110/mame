/***************************************************************************

Popeye memory map (preliminary)

0000-7fff  ROM

8000-87ff  RAM
8c00       background x position
8c01       background y position
8c02       ?
8c03       bit 3: background palette bank
           bit 0-2: sprite palette bank
8c04-8e7f  sprites
8f00-8fff  RAM (stack)

a000-a3ff  Text video ram
a400-a7ff  Text Attribute

c000-cfff  Background bitmap. Accessed as nibbles: bit 7 selects which of the
           two nibbles should be written to.


I/O 0  ;AY-3-8910 Control Reg.
I/O 1  ;AY-3-8910 Data Write Reg.
        write to port B: select bit of DSW2 to read in bit 7 of port A (0-2-4-6-8-a-c-e)
I/O 3  ;AY-3-8910 Data Read Reg.
        read from port A: bit 0-5 = DSW1  bit 7 = bit of DSW1 selected by port B

        DSW1
		bit 0-3 = coins per play (0 = free play)
		bit 4-5 = ?

		DSW2
        bit 0-1 = lives
		bit 2-3 = difficulty
		bit 4-5 = bonus
		bit 6 = demo sounds
		bit 7 = cocktail/upright (0 = upright)

I/O 2  ;bit 0 Coin in 1
        bit 1 Coin in 2
        bit 2 Coin in 3 = 5 credits
        bit 3
        bit 4 Start 2 player game
        bit 5 Start 1 player game
        bit 6
        bit 7

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "sndhrdw/8910intf.h"



extern unsigned char *popeye_videoram,*popeye_colorram,*popeye_backgroundram;
extern unsigned char *popeye_background_pos,*popeye_palette_bank;
extern void popeye_backgroundram_w(int offset,int data);
extern void popeye_palettebank_w(int offset,int data);
extern void popeye_vh_convert_color_prom(unsigned char *palette, unsigned char *colortable,const unsigned char *color_prom);
extern void popeye_vh_screenrefresh(struct osd_bitmap *bitmap);

extern int popeye_sh_start(void);



static struct MemoryReadAddress readmem[] =
{
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8c00, 0x8e7f, MRA_RAM },
	{ 0x8f00, 0x8fff, MRA_RAM },
	{ 0x0000, 0x7fff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8c04, 0x8e7f, MWA_RAM, &spriteram },
	{ 0x8f00, 0x8fff, MWA_RAM },
	{ 0xa000, 0xa3ff, MWA_RAM, &popeye_videoram },
	{ 0xa400, 0xa7ff, MWA_RAM, &popeye_colorram },
	{ 0xc000, 0xcfff, popeye_backgroundram_w, &popeye_backgroundram },
	{ 0x8c00, 0x8c01, MWA_RAM, &popeye_background_pos },
	{ 0x8c03, 0x8c03, popeye_palettebank_w, &popeye_palette_bank },
	{ 0x0000, 0x7fff, MWA_ROM },
	{ -1 }	/* end of table */
};



static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, input_port_0_r },
	{ 0x01, 0x01, input_port_1_r },
	{ 0x02, 0x02, input_port_2_r },
	{ 0x03, 0x03, AY8910_read_port_0_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};





static struct InputPort input_ports[] =
{
	{	/* IN0 */
		0x00,
		{ OSD_KEY_RIGHT, OSD_KEY_LEFT, OSD_KEY_UP, OSD_KEY_DOWN,
				OSD_KEY_CONTROL, OSD_KEY_E, OSD_KEY_Q, OSD_KEY_W },
		{ OSD_JOY_RIGHT, OSD_JOY_LEFT, OSD_JOY_UP, OSD_JOY_DOWN,
                                OSD_JOY_FIRE1, OSD_JOY_FIRE2, OSD_JOY_FIRE3, OSD_JOY_FIRE4 }
	},
	{	/* IN1 */
		0x00,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{	/* IN2 */
		0x00,
		{ OSD_KEY_8, OSD_KEY_9, OSD_KEY_1,OSD_KEY_2,
                      OSD_KEY_0, OSD_KEY_5, OSD_KEY_4,OSD_KEY_3 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{	/* DSW1 */
		0x3f,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{	/* DSW2 */
		0x3d,
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	{ -1 }	/* end of table */
};


static struct KEYSet keys[] =
{
        { 0, 2, "MOVE UP" },
        { 0, 1, "MOVE LEFT"  },
        { 0, 0, "MOVE RIGHT" },
        { 0, 3, "MOVE DOWN" },
        { 0, 4, "FIRE1" },
        { 0, 6, "FIRE2" },
        { 0, 7, "FIRE3" },
        { 0, 5, "FIRE4" },
        { -1 }
};


static struct DSW dsw[] =
{
	{ 4, 0x03, "LIVES", { "4", "3", "2", "1" }, 1 },
	{ 4, 0x30, "BONUS", { "NONE", "80000", "60000", "40000" }, 1 },
	{ 4, 0x0c, "DIFFICULTY", { "HARDEST", "HARD", "MEDIUM", "EASY" }, 1 },
	{ 4, 0x40, "DEMO SOUNDS", { "ON", "OFF" }, 1 },
	{ 3, 0x10, "SW1 5", { "ON", "OFF" }, 1 },
	{ 3, 0x20, "SW1 6", { "ON", "OFF" }, 1 },
	{ -1 }
};



static struct GfxLayout charlayout =
{
	16,16,	/* 16*16 characters (8*8 doubled) */
	256,	/* 256 characters */
	1,	/* 1 bit per pixel (there are two bitplanes in the ROM, but only one is used) */
	{ 0 },
	{ 7,7, 6,6, 5,5, 4,4, 3,3, 2,2, 1,1, 0,0 },	/* pretty straightforward layout */
	{ 0*8,0*8, 1*8,1*8, 2*8,2*8, 3*8,3*8, 4*8,4*8, 5*8,5*8, 6*8,6*8, 7*8,7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	256,	/* 256 sprites */
	2,	/* 2 bits per pixel */
	{ 0, 0x4000*8 },	/* the two bitplanes are separated in different files */
	{7+(0x2000*8),6+(0x2000*8),5+(0x2000*8),4+(0x2000*8),
     3+(0x2000*8),2+(0x2000*8),1+(0x2000*8),0+(0x2000*8),
   7,6,5,4,3,2,1,0 },
	{ 15*8, 14*8, 13*8, 12*8, 11*8, 10*8, 9*8, 8*8,
    7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8, },
	16*8	/* every sprite takes 16 consecutive bytes */
};
/* there's nothing here, this is just a placeholder to let the video hardware */
/* pick the background color table. */
static struct GfxLayout fakelayout =
{
	1,1,
	0,
	1,
	{ 0 },
	{ 0 },
	{ 0 },
	0
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0800, &charlayout,        0, 32 },	/* chars */
	{ 1, 0x1000, &spritelayout,32*2+32, 64 },	/* sprites */
	{ 1, 0x2000, &spritelayout,32*2+32, 64 },	/* sprites */
	{ 0, 0,      &fakelayout,     32*2, 32 },	/* background bitmap */
	{ -1 } /* end of array */
};



static unsigned char color_prom[] =
{
	/* char palette */
	0x78,0xF0,0xF6,0xA4,0x07,0xD0,0x2F,0xAD,0xFF,0x36,0x3F,0x73,0xFF,0xAF,0xD0,0x00,
	0x78,0xF0,0xF6,0xA4,0x07,0xD0,0x2F,0xAD,0xFF,0x36,0x3F,0x73,0xFF,0xAF,0xD0,0x00,
	/* background palette */
	0x00,0x0A,0x2A,0x01,0x15,0x27,0x05,0x5B,0x40,0x48,0x9B,0xAE,0x54,0x53,0xB7,0x4D,
	0x00,0x89,0x05,0x40,0xFF,0x0F,0x01,0x16,0x00,0x0B,0xB7,0x2F,0x03,0x07,0xED,0xFF,
	/* sprite palette */
	/* low 4 bits */
	0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x07,0x0F,0x0A,0x00,0x07,0x0F,0x00,
	0x00,0x07,0x0F,0x08,0x00,0x0C,0x07,0x00,0x00,0x06,0x08,0x00,0x00,0x03,0x0F,0x04,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x07,0x0F,0x0B,0x00,0x07,0x0F,0x0F,
	0x00,0x07,0x06,0x06,0x00,0x04,0x06,0x00,0x00,0x06,0x08,0x00,0x00,0x0C,0x0F,0x0F,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x07,0x0F,0x0B,0x00,0x07,0x0F,0x08,
	0x00,0x08,0x00,0x0F,0x00,0x02,0x0F,0x0F,0x00,0x06,0x08,0x00,0x00,0x02,0x0F,0x07,
	0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x07,0x00,0x05,0x0F,0x0F,0x00,0x07,0x00,0x07,
	0x00,0x07,0x0F,0x05,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x07,0x0F,0x0A,0x00,0x07,0x0F,0x00,
	0x00,0x07,0x0F,0x08,0x00,0x0C,0x07,0x00,0x00,0x06,0x08,0x00,0x00,0x03,0x0F,0x04,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x07,0x0F,0x0B,0x00,0x07,0x0F,0x0F,
	0x00,0x07,0x06,0x06,0x00,0x04,0x06,0x00,0x00,0x06,0x08,0x00,0x00,0x0C,0x0F,0x0F,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x07,0x0F,0x0B,0x00,0x07,0x0F,0x08,
	0x00,0x08,0x00,0x0F,0x00,0x02,0x0F,0x0F,0x00,0x06,0x08,0x00,0x00,0x02,0x0F,0x07,
	0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x07,0x00,0x05,0x0F,0x0F,0x00,0x07,0x00,0x07,
	0x00,0x07,0x0F,0x05,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F,
	/* high 4 bits */
	0x00,0x00,0x00,0x00,0x00,0x08,0x0A,0x0F,0x00,0x00,0x0A,0x00,0x00,0x00,0x0A,0x0A,
	0x00,0x00,0x0F,0x03,0x00,0x00,0x0B,0x0C,0x00,0x0F,0x03,0x0C,0x00,0x00,0x0A,0x08,
	0x00,0x00,0x00,0x00,0x00,0x08,0x0A,0x0F,0x00,0x00,0x0A,0x00,0x00,0x00,0x0A,0x0F,
	0x00,0x00,0x0F,0x03,0x00,0x01,0x0F,0x0C,0x00,0x0F,0x03,0x0C,0x00,0x00,0x0A,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x0F,0x00,0x00,0x0A,0x00,0x00,0x00,0x0A,0x09,
	0x00,0x03,0x00,0x03,0x00,0x00,0x06,0x0F,0x00,0x0F,0x03,0x0C,0x00,0x00,0x0A,0x04,
	0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x02,0x00,0x01,0x0A,0x0F,0x00,0x00,0x00,0x00,
	0x00,0x00,0x0A,0x01,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F,
	0x00,0x00,0x00,0x00,0x00,0x08,0x04,0x0F,0x00,0x00,0x0A,0x00,0x00,0x00,0x0A,0x0A,
	0x00,0x00,0x0F,0x03,0x00,0x00,0x0B,0x0C,0x00,0x0F,0x03,0x0C,0x00,0x00,0x0A,0x08,
	0x00,0x00,0x00,0x00,0x00,0x08,0x04,0x0F,0x00,0x00,0x0A,0x00,0x00,0x00,0x0A,0x0F,
	0x00,0x00,0x0F,0x03,0x00,0x01,0x0F,0x0C,0x00,0x0F,0x03,0x0C,0x00,0x00,0x0A,0x01,
	0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x0F,0x00,0x00,0x0A,0x00,0x00,0x00,0x0A,0x09,
	0x00,0x03,0x00,0x03,0x00,0x00,0x06,0x0F,0x00,0x0F,0x03,0x0C,0x00,0x00,0x0A,0x04,
	0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x02,0x00,0x01,0x0A,0x0F,0x00,0x00,0x00,0x00,
	0x00,0x00,0x0A,0x01,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F,0x00,0x0F,0x0F,0x0F
};



static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz */
			0,
			readmem,writemem,readport,writeport,
			nmi_interrupt,1
		}
	},
	60,
	0,

	/* video hardware */
	32*16, 30*16, { 0*16, 32*16-1, 1*16, 29*16-1 },
	gfxdecodeinfo,
	256,32*2+32+64*4,
	popeye_vh_convert_color_prom,

	0,
	generic_vh_start,
	generic_vh_stop,
	popeye_vh_screenrefresh,

	/* sound hardware */
	0,
	0,
	popeye_sh_start,
	AY8910_sh_stop,
	AY8910_sh_update
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( popeyebl_rom )
	ROM_REGION(0x10000)	/* 64k for code */
	ROM_LOAD( "po1",          0x0000, 0x2000 )
	ROM_LOAD( "po2",          0x2000, 0x2000 )
	ROM_LOAD( "po3",          0x4000, 0x2000 )
	ROM_LOAD( "po4",          0x6000, 0x2000 )
	ROM_LOAD( "po_d1-e1.bin", 0xe000, 0x0020 )	/* protection PROM */

	ROM_REGION(0x9000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "po5", 0x0000, 0x1000 )
	ROM_LOAD( "po6", 0x1000, 0x2000 )
	ROM_LOAD( "po7", 0x3000, 0x2000 )
	ROM_LOAD( "po8", 0x5000, 0x2000 )
	ROM_LOAD( "po9", 0x7000, 0x2000 )
ROM_END



static int hiload(const char *name)
{
	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x8209],"\x00\x26\x03",3) == 0 &&
			memcmp(&RAM[0x8221],"\x50\x11\x02",3) == 0)
	{
		FILE *f;


		if ((f = fopen(name,"rb")) != 0)
		{
			int i;


			fread(&RAM[0x8200],1,6+6*5,f);

			i = RAM[0x8201];

			RAM[0x8fed] = RAM[0x8200+i-2];
			RAM[0x8fee] = RAM[0x8200+i-1];
			RAM[0x8fef] = RAM[0x8200+i];

			RAM[0x8f32] = RAM[0x8200+i] >> 4;
			RAM[0x8f33] = RAM[0x8200+i] & 0x0f;
			RAM[0x8f34] = RAM[0x8200+i-1] >> 4;
			RAM[0x8f35] = RAM[0x8200+i-1] & 0x0f;
			RAM[0x8f36] = RAM[0x8200+i-2] >> 4;
			RAM[0x8f37] = RAM[0x8200+i-2] & 0x0f;

			fclose(f);
		}

		return 1;
	}
	else return 0;	/* we can't load the hi scores yet */
}



static void hisave(const char *name)
{
	FILE *f;


	if ((f = fopen(name,"wb")) != 0)
	{
		fwrite(&RAM[0x8200],1,6+6*5,f);
		fclose(f);
	}
}



struct GameDriver popeyebl_driver =
{
	"popeyebl",
	&machine_driver,

	popeyebl_rom,
	0, 0,
	0,

	input_ports, dsw, keys,

	color_prom, 0, 0,
	{ 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,	/* numbers */
		0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,	/* letters */
		0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23 },
	0x0c, 0x0a,
	16*13, 16*16, 0x04,

	hiload,hisave
};
