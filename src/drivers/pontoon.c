/***************************************************************************

Pontoon Memory Map (preliminary)

Enter switch test mode by holding down the Hit key, and when the
crosshatch pattern comes up, release it and push it again.

After you get into Check Mode (F2), press the Hit key to switch pages.


Memory Mapped:

0000-5fff   R	ROM
6000-67ff   RW  Battery Backed RAM
8000-83ff   RW  Video RAM
8400-87ff   RW  Color RAM
				Bits 0-3 - color
 				Bits 4-5 - character bank
				Bit  6   - flip x
				Bit  7   - Is it used?
a000		R	Input Port 0
a001		R	Input Port 1
a002		R	Input Port 2
a001		 W  Control Port 0
a002		 W  Control Port 1

I/O Ports:
00			RW  YM2149 Data	Port
				Port A - DSW #1
				Port B - DSW #2
01           W  YM2149 Control Port


TODO:

- What do the control ports do?
- CPU speed/ YM2149 frequencies
- Input ports need to be cleaned up


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



void pontoon_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void pontoon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);




static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x67ff, MRA_RAM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xa000, 0xa000, input_port_0_r },
	{ 0xa001, 0xa001, input_port_1_r },
	{ 0xa002, 0xa002, input_port_2_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x67ff, MWA_RAM },
	{ 0x8000, 0x83ff, videoram_w, &videoram, &videoram_size },
	{ 0x8400, 0x87ff, colorram_w, &colorram },
	{ 0xa001, 0xa002, MWA_NOP },  /* Probably lights and stuff */
	{ -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x00, AY8910_read_port_0_r },
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x00, 0x00, AY8910_write_port_0_w },
	{ 0x01, 0x01, AY8910_control_port_0_w },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( input_ports )
	PORT_START      /* IN0 */
	PORT_BITX(    0x01, 0x01, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Check Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x01, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_COIN3 )
	PORT_BITX(0x04, IP_ACTIVE_LOW,  0, "Reset All", OSD_KEY_F5, IP_JOY_NONE, 0)  /* including battery backed RAM */
	PORT_BITX(0x08, IP_ACTIVE_LOW,  0, "Clear Stats", OSD_KEY_F6, IP_JOY_NONE, 0)
	PORT_BITX(0x10, IP_ACTIVE_LOW,  0, "Call Attendant", OSD_KEY_6, IP_JOY_NONE, 0)
	PORT_BITX(0x20, IP_ACTIVE_LOW,  0, "Reset Hopper", OSD_KEY_7, IP_JOY_NONE, 0)
	PORT_BIT( 0xc0, IP_ACTIVE_LOW,  IPT_UNUSED ) /*  ne donne rien */

	PORT_START      /* IN1 */
	PORT_BIT(0x07, IP_ACTIVE_LOW,  IPT_UNUSED ) /* ne donne rien */
	PORT_BITX(0x08, IP_ACTIVE_LOW,  IPT_BUTTON1, "Bonus Game", IP_KEY_DEFAULT, IP_JOY_DEFAULT, 0)
	PORT_BITX(0x10, IP_ACTIVE_LOW,  IPT_BUTTON2, "Stand",      IP_KEY_DEFAULT, IP_JOY_DEFAULT, 0)
	PORT_BITX(0x20, IP_ACTIVE_LOW,  IPT_BUTTON3, "Hit",        IP_KEY_DEFAULT, IP_JOY_DEFAULT, 0)
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_BUTTON5 )
	PORT_BITX(0x80, IP_ACTIVE_LOW,  IPT_BUTTON4, "Pay Out",    IP_KEY_DEFAULT, IP_JOY_DEFAULT, 0)

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_UNKNOWN )   /* Token Drop */
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,  IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_UNKNOWN )	/* Token In */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* Overflow */
	PORT_BITX(0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Coin Out", OSD_KEY_8, IP_JOY_NONE, 0 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNUSED )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, "Winning Percentage", IP_KEY_NONE )
	PORT_DIPSETTING(    0x06, "70%" )
	PORT_DIPSETTING(    0x05, "74%" )
	PORT_DIPSETTING(    0x04, "78%" )
	PORT_DIPSETTING(    0x03, "82%" )
	PORT_DIPSETTING(    0x02, "86%" )
	PORT_DIPSETTING(    0x07, "90%" )
	PORT_DIPSETTING(    0x01, "94%" )
	PORT_DIPSETTING(    0x00, "98%" )
	PORT_DIPNAME( 0x08, 0x08, "Dip 1  Switch 3", IP_KEY_NONE )   /* Doesn't appear on the test menu */
	PORT_DIPSETTING(    0x08, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x10, 0x10, "Dip 1  Switch 4", IP_KEY_NONE )   /* Doesn't appear on the test menu */
	PORT_DIPSETTING(    0x10, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x20, 0x20, "Dip 1  Switch 5", IP_KEY_NONE )   /* Doesn't appear on the test menu */
	PORT_DIPSETTING(    0x20, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x60, 0x20, "Payment Method", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Credit In/Coin Out" )
	PORT_DIPSETTING(    0x20, "Coin In/Coin Out" )
	PORT_DIPSETTING(    0x40, "Credit In/Credit Out" )
  /*PORT_DIPSETTING(    0x60, "Credit In/Coin Out" ) */
	PORT_DIPNAME( 0x80, 0x80, "Reset All Switch", IP_KEY_NONE )
	PORT_DIPSETTING(    0x80, "Disable" )
	PORT_DIPSETTING(    0x00, "Enable" )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x07, 0x06, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x04, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x06, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x03, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x02, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x01, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x00, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0x07, "1 Coin/6 Credits" )
	PORT_DIPNAME( 0x38, 0x30, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x28, "2 Coins/2 Credits" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x20, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x38, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x18, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x10, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0x08, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x00, "1 Coin/10 Credits" )
	PORT_DIPNAME( 0x40, 0x40, "Coin C", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "1 Coin/10 Credits" )
	PORT_DIPSETTING(    0x40, "1 Coin/1 Credit" )
	PORT_DIPNAME( 0x80, 0x00, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Off" )
	PORT_DIPSETTING(    0x80, "On" )
INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 256 characters */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8    /* every char takes 32 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout, 0, 16 },
	{ 1, 0x2000, &charlayout, 0, 16 },
	{ 1, 0x4000, &charlayout, 0, 16 },
	{ 1, 0x6000, &charlayout, 0, 16 },
	{ -1 } /* end of array */
};


static struct AY8910interface ay8910_interface =
{
	1,	/* 2 chips */
	4608000,	/* 18.432000 / 4 (???) */
	{ 255 },
	{ input_port_3_r },
	{ input_port_4_r },
	{ 0 },
	{ 0 }
};


static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4608000,	/* 18.432000 / 4 (???) */
			0,
			readmem,writemem,readport,writeport,
			interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 256,
	pontoon_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	pontoon_vh_screenrefresh,

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
ROM_START( pontoon_rom )
	ROM_REGION(0x10000)         /* 64k for code */
	ROM_LOAD( "ponttekh.001",   0x0000, 0x4000, 0x1f8c1b38 )
	ROM_LOAD( "ponttekh.002",   0x4000, 0x2000, 0xbefb4f48 )

	ROM_REGION_DISPOSE(0x8000)  /* temporary space for graphics */
	ROM_LOAD( "ponttekh.003",   0x0000, 0x2000, 0xa6a91b3d )
	ROM_LOAD( "ponttekh.004",   0x2000, 0x2000, 0x976ed924 )
	ROM_LOAD( "ponttekh.005",   0x4000, 0x2000, 0x2b8e8ca7 )
	ROM_LOAD( "ponttekh.006",   0x6000, 0x2000, 0x6bc23965 )

	ROM_REGION(0x0300)           /* color proms */
	ROM_LOAD( "pon24s10.003",   0x0000, 0x0100, 0x4623b7f3 )  /* red component */
	ROM_LOAD( "pon24s10.002",   0x0100, 0x0100, 0x117e1b67 )  /* green component */
	ROM_LOAD( "pon24s10.001",   0x0200, 0x0100, 0xc64ecee8 )  /* blue component */
ROM_END


/* Load/Save the battery backed up RAM */
static int hiload(void)
{
	void *f;
	unsigned char *RAM = Machine->memory_region[Machine->drv->cpu[0].memory_region];


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
	{
		osd_fread(f,&RAM[0x6000],0x0800);
		osd_fclose(f);
	}
	else
	{
		memset(&RAM[0x6000],0xff,0x0800);
	}

	return 1;
}



static void hisave(void)
{
	void *f;
	unsigned char *RAM = Machine->memory_region[Machine->drv->cpu[0].memory_region];


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
		osd_fwrite(f,&RAM[0x6000],0x0800);
		osd_fclose(f);
	}
}



struct GameDriver pontoon_driver =
{
	__FILE__,
	0,
	"pontoon",
	"Pontoon",
	"1985",
	"Tehkan",
	"Zsolt Vasvari/nGerald Coy",
	0,
	&machine_driver,
	0,

	pontoon_rom,
	0,
	0,
	0,
	0,      /* sound_prom */

	input_ports,

	PROM_MEMORY_REGION(2), 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,

	hiload, hisave
};
