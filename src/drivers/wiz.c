/***************************************************************************

Wiz memory map (preliminary)

This board is similar to a Galaxian board in the way it handles scrolling
and sprites, but the similarities pretty much end there. The most notable
difference is that there are 2 independently scrollable playfields.

Strangely, one of the foreground character banks have a Seibu logo, the
other one a Taito logo. There are other differences as well, so you can't
just switch one for the other.


Main CPU:

0000-BFFF  ROM
C000-C7FF  RAM
D000-D3FF  Video RAM (Foreground)
D400-D7FF  Color RAM (Foreground)
D800-D83F  Scroll RAM (Foreground)
D840-D85F  Sprite RAM 1
E000-E3FF  Video RAM (Background)
E400-E7FF  Color RAM (Background) (I don't think it's used)
E800-E83F  Scroll RAM (Background)
E840-E85F  Sprite RAM 2

I/O read:
f000 DIP SW#1
f008 DIP SW#2
f010 Input Port 1
f010 Input Port 2

I/O write:
c800 Puslated to 1 when a coin is inserted into Chute A
c801 Puslated to 1 when a coin is inserted into Chute B
f000 Sprite bank select
f001 NMI enable
f002 \ (?) Set based on current level (C6E9)
f003 / (?) Possibly palette select
f004 \ Character bank select
f005 /
f800 Sound Command write
f818 (?) Sound (Is there some other sound circuit??)


Sound CPU:

0000-1FFF  ROM
2000-23FF  RAM

I/O read:
7000 Sound Command Read

I/O write:
4000 AY8910 Control Port #1
4001 AY8910 Write Port #1
5000 AY8910 Control Port #2
5001 AY8910 Write Port #2
6000 AY8910 Control Port #3
6001 AY8910 Write Port #3
7000 NMI enable

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *wiz_videoram2;
extern unsigned char *wiz_colorram2;
extern unsigned char *wiz_attributesram;
extern unsigned char *wiz_attributesram2;
extern unsigned char *wiz_sprite_bank;

void wiz_background_bank_select_w (int offset, int data);
void wiz_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void wiz_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void wiz_flipx_w(int offset,int data);
void wiz_flipy_w(int offset,int data);


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xd000, 0xd85f, MRA_RAM },
	{ 0xe000, 0xe85f, MRA_RAM },
	{ 0xf000, 0xf000, input_port_0_r },
	{ 0xf008, 0xf008, input_port_1_r },
	{ 0xf010, 0xf010, input_port_2_r },
	{ 0xf018, 0xf018, input_port_3_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xc800, 0xc801, MWA_NOP },
	{ 0xd000, 0xd3ff, MWA_RAM, &wiz_videoram2 },
	{ 0xd400, 0xd7ff, MWA_RAM, &wiz_colorram2 },
	{ 0xd800, 0xd83f, MWA_RAM, &wiz_attributesram2 },
	{ 0xd840, 0xd85f, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xe000, 0xe3ff, videoram_w, &videoram, &videoram_size },
	//{ 0xe400, 0xe7ff, colorram_w, &colorram },  // Always 0
	{ 0xe400, 0xe7ff, MWA_RAM },
	{ 0xe800, 0xe83f, MWA_RAM, &wiz_attributesram },
	{ 0xe840, 0xe85f, MWA_RAM, &spriteram_2, &spriteram_2_size },
	{ 0xf000, 0xf000, MWA_RAM, &wiz_sprite_bank },
	{ 0xf001, 0xf001, interrupt_enable_w },
	{ 0xf002, 0xf003, MWA_NOP },  /* Unknown (Palette select?) */
	{ 0xf004, 0xf005, wiz_background_bank_select_w },
	{ 0xf006, 0xf006, wiz_flipx_w },
	{ 0xf007, 0xf007, wiz_flipy_w },
	{ 0xf800, 0xf800, soundlatch_w },
	{ 0xf818, 0xf818, MWA_NOP },
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x23ff, MRA_RAM },
	{ 0x7000, 0x7000, soundlatch_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x4000, 0x4000, AY8910_control_port_0_w },
	{ 0x4001, 0x4001, AY8910_write_port_0_w },
	{ 0x5000, 0x5000, AY8910_control_port_1_w },
	{ 0x5001, 0x5001, AY8910_write_port_1_w },
	{ 0x6000, 0x6000, AY8910_control_port_2_w },
	{ 0x6001, 0x6001, AY8910_write_port_2_w },
	{ 0x7000, 0x7000, interrupt_enable_w },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( input_ports )
	PORT_START      /* DSW 0 */
	PORT_DIPNAME( 0x07, 0x00, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x07, "5 Coins/1 Credit" )
	PORT_DIPSETTING(    0x03, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x05, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x01, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x00, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x04, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x02, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x06, "1 Coin/5 Credits" )
	PORT_DIPNAME( 0x18, 0x00, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x08, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x00, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x18, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0x10, "1 Coin/2 Credits" )
	PORT_DIPNAME( 0x20, 0x00, "Unknown 1", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x20, "On" )
	PORT_DIPNAME( 0x40, 0x00, "Unknown 2", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x40, "On" )
	PORT_DIPNAME( 0x80, 0x00, "Free Play", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x80, "On" )

	PORT_START      /* DSW 1 */
	PORT_DIPNAME( 0x01, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x01, "Cocktail" )
	PORT_DIPNAME( 0x06, 0x00, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x06, "4" )
	PORT_DIPNAME( 0x18, 0x10, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x18, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "255", IP_KEY_NONE, IP_JOY_NONE, 0 )
	PORT_DIPNAME( 0x60, 0x00, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "10000 30000" )
	PORT_DIPSETTING(    0x20, "20000 40000" )
	PORT_DIPSETTING(    0x40, "30000 60000" )
	PORT_DIPSETTING(    0x60, "40000 80000" )
	PORT_BITX(    0x80, 0x00, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x80, "On" )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	3,      /* 3 bits per pixel */
	{ 0, 0x2000*8, 0x4000*8 }, /* the three bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};


static struct GfxLayout charlayout2 =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	3,      /* 3 bits per pixel */
	{ 0, 0x4000*8, 0x8000*8 }, /* the three bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8     /* every char takes 8 consecutive bytes */
};


static struct GfxLayout spritelayout2 =
{
	16,16,  /* 16*16 sprites */
	128,    /* 128 sprites */
	3,      /* 3 bits per pixel */
	{ 0, 0x4000*8, 0x8000*8 }, /* the three bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	  8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8     /* every sprites takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	128,    /* 128 sprites */
	3,      /* 3 bits per pixel */
	{ 0, 0x2000*8, 0x4000*8 }, /* the three bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	 8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	  16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8     /* every sprite takes 32 consecutive bytes */
};


// I don't know how the 32 colors are used. I'm making the
// forest and the title screen green/brown, and the ice and sky levels
// white/blue. What we need is screenshots.
static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,     0, 8 },
	{ 1, 0x0800, &charlayout,     0, 8 },
	{ 1, 0x1000, &spritelayout,   0, 8 },
	{ 1, 0x8000, &charlayout2,   24, 1 },
	{ 1, 0x6000, &charlayout2,    0, 1 },
	{ 1, 0x6800, &charlayout2,   24, 1 },
	{ 1, 0x8800, &charlayout2,    0, 1 },
	{ 1, 0x7000, &spritelayout2,  0, 8 },
	{ 1, 0x9000, &spritelayout2,  0, 8 },
	{ 1, 0x8000, &spritelayout2,  0, 8 },
	{ -1 } /* end of array */
};


static struct AY8910interface ay8910_interface =
{
	3,      /* 3 chips */
	14318000/8,	/* ? */
	{ 10, 10, 10 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};


static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			18432000/6,     /* 3.072 Mhz */
			0,
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			14318000/8,     /* ? */
			3,
			sound_readmem,sound_writemem,0,0,
			nmi_interrupt,4
		}

	},
	60, 2500,       /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256,32*8,
	wiz_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	wiz_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( wiz_rom )
	ROM_REGION(0x10000)     /* 64k for code */
	ROM_LOAD( "ic07_01.bin",  0x0000, 0x4000, 0xc05f2c78 )
	ROM_LOAD( "ic05_03.bin",  0x4000, 0x4000, 0x7978d879 )
	ROM_LOAD( "ic06_02.bin",  0x8000, 0x4000, 0x9c406ad2 )

	ROM_REGION_DISPOSE(0x12000)      /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ic14_06.bin",  0x0000, 0x2000, 0xb398e142 )
	ROM_LOAD( "ic13_05.bin",  0x2000, 0x2000, 0x2868e6a5 )
	ROM_LOAD( "ic12_04.bin",  0x4000, 0x2000, 0x8969acdd )
	ROM_LOAD( "ic01_09.bin",  0x6000, 0x4000, 0x4d86b041 )
	ROM_LOAD( "ic02_08.bin",  0xa000, 0x4000, 0xede77d37 )
	ROM_LOAD( "ic03_07.bin",  0xe000, 0x4000, 0x297c02fc )

	ROM_REGION(0x0300)      /* color proms */
	/* These proms are all $200 bytes but only the first $100 is used */
	ROM_LOAD( "ic23_3-1.bin", 0x0000, 0x0100, 0x2dd52fb2 ) /* palette red component */
	ROM_LOAD( "ic23_3-2.bin", 0x0100, 0x0100, 0x8c2880c9 ) /* palette green component */
	ROM_LOAD( "ic23_3-3.bin", 0x0200, 0x0100, 0xa488d761 ) /* palette blue component */

	ROM_REGION(0x10000)     /* 64k for the audio CPU */
	ROM_LOAD( "ic57_10.bin",  0x0000, 0x2000, 0x8a7575bd )
ROM_END



static int hiload(void)
{
	unsigned char *RAM = Machine->memory_region[Machine->drv->cpu[0].memory_region];


	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0xc04e],"\x4d\x43",2) == 0)
	{
		void *f;


		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread(f,&RAM[0xc01e],0x32);
			osd_fclose(f);
		}

		return 1;
	}
	else return 0;  /* we can't load the hi scores yet */
}



static void hisave(void)
{
	void *f;
	unsigned char *RAM = Machine->memory_region[Machine->drv->cpu[0].memory_region];


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
		osd_fwrite(f,&RAM[0xc01e],0x32);
		osd_fclose(f);
	}
}



struct GameDriver wiz_driver =
{
	__FILE__,
	0,
	"wiz",
	"Wiz",
	"1985",
	"Seibu Kaihatsu",
	"Zsolt Vasvari",
	0,
	&machine_driver,
	0,

	wiz_rom,
	0, 0,
	0,
	0,      /* sound_prom */

	input_ports,

	PROM_MEMORY_REGION(2), 0, 0,
	ORIENTATION_ROTATE_270,

	hiload, hisave
};
