/***************************************************************************

Klax Memory Map
---------------

KLAX 68010 MEMORY MAP

Program ROM             000000-05FFFF   R    D[15:0]
Program ROM slapstic    058000-05FFFF   R    D[15:0]   (not used!)

EEPROM                  0E0001-0E0FFF  R/W   D[7:0]    (odd bytes only)
UNLOCK EEPROM           1Fxxxx          W
Watch Dog               2E0000          W    xx        (128 msec. timeout)

Color RAM Motion Object 3E0000-3E03FE  R/W   D[15:8]
Color RAM Playfield     3E0400-3E07FE  R/W   D[15:8]

Playfield Picture RAM   3F0000-3F0EFF  R/W   D[15:0]
MOB config              3F0F00-3F0F70  R/W   D[15:0]
SLIP pointers           3F0F80-3F0FFF  R/W   M.O. link pointers
Playfield palette AM    3F1000-3F1FFF  R/W   D[11:8]
Motion Object RAM       3F2000-3F27FF  R/W   D[15:0]
(Link, Picture, H-Pos, V-Pos, Link... etc.)
Working RAM             3F2800-3F3FFF  R/W   D[15:0]

Player 1 Input (left)   260000          R    D[15:12],D8 Active lo
Player 2 Input (right)  260002          R    D[15:12],D8 Active lo
      D8:    flip
      D12:   right
      D13:   left
      D14:   down
      D15:   up

Status inputs           260000          R    D11,D1,D0
      D0:    coin 1 (left) Active lo
      D1:    coin 2 (right) Active lo
      D11:   self-test Active lo

LATCH                   260000          W    D[12:8]
      D8:    ADPCM chip reset (active lo)
      D9:    Spare
      D10:   Coin Counter 2 (right)
      D11:   Coin Counter 1 (left)
      D12:   Spare
      D13:   Color RAM bank select
  NOTE: RESET clears this latch

4ms Interrupt ack.      360000          W    xx

ADPCM chip              270000         R/W   D[7:0]

****************************************************************************/



#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"
#include "sndhrdw/adpcm.h"


int klax_playfieldram_r (int offset);
int klax_paletteram_r (int offset);

void klax_latch_w (int offset, int data);
void klax_playfieldram_w (int offset, int data);
void klax_paletteram_w (int offset, int data);

int klax_interrupt (void);

void klax_init_machine (void);

int klax_vh_start (void);
void klax_vh_stop (void);

void klax_vh_screenrefresh (struct osd_bitmap *bitmap);

void klax_update_display_list (int scanline);



/*************************************
 *
 *		Misc. functions
 *
 *************************************/

void klax_init_machine (void)
{
	klax_latch_w (0, 0);
}


int klax_input_r (int offset)
{
	if (offset == 0)
		return input_port_0_r (offset) + (input_port_1_r (offset) << 8);
	else
		return input_port_2_r (offset) + (input_port_3_r (offset) << 8);
}


int klax_adpcm_r (int offset)
{
	return OKIM6295_status_r (offset) | 0xff00;
}


void klax_adpcm_w (int offset, int data)
{
	if (!(data & 0x00ff0000))
		OKIM6295_data_w (offset, data & 0xff);
}


void klax_update (int param)
{
	klax_update_display_list (param);
	param += 8;
	if (param < 240)
		timer_set (8.0 * cpu_getscanlineperiod (), param, klax_update);
}


int klax_interrupt (void)
{
	timer_set (TIME_IN_USEC (Machine->drv->vblank_duration), 0, klax_update);
	return 4;
}



/*************************************
 *
 *		Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress klax_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x0e0000, 0x0e0fff, atarigen_eeprom_r, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0x260000, 0x260003, klax_input_r },
	{ 0x270000, 0x270003, klax_adpcm_r },
	{ 0x3e0000, 0x3e07ff, klax_paletteram_r, &atarigen_paletteram, &atarigen_paletteram_size },
	{ 0x3f0000, 0x3f1fff, klax_playfieldram_r, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0x3f2000, 0x3f27ff, MRA_BANK3, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0x3f2800, 0x3f3fff, MRA_BANK2 },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress klax_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x0e0000, 0x0e0fff, atarigen_eeprom_w },
	{ 0x1f0000, 0x1fffff, atarigen_eeprom_enable_w },
	{ 0x260000, 0x260003, klax_latch_w },
	{ 0x270000, 0x270003, klax_adpcm_w },
	{ 0x2e0000, 0x2e0003, watchdog_reset_w },
	{ 0x360000, 0x360003, MWA_NOP },
	{ 0x3e0000, 0x3e07ff, klax_paletteram_w },
	{ 0x3f0000, 0x3f1fff, klax_playfieldram_w },
	{ 0x3f2000, 0x3f27ff, MWA_BANK3 },
	{ 0x3f2800, 0x3f3fff, MWA_BANK2 },
	{ -1 }  /* end of table */
};


/*************************************
 *
 *		Port definitions
 *
 *************************************/

INPUT_PORTS_START( klax_ports )
	PORT_START		/* IN0 low */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN0 high */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x06, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )

	PORT_START		/* IN1 low */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 high */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BITX(    0x08, 0x08, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Self Test", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x08, "Off")
	PORT_DIPSETTING(    0x00, "On")
	PORT_BIT( 0x06, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
INPUT_PORTS_END



/*************************************
 *
 *		Graphics definitions
 *
 *************************************/

static struct GfxLayout klax_pflayout =
{
	8,8,	  /* 8*8 sprites */
	8192,   /* 8192 of them */
	4,		  /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0, 4, 0x30000*8+0, 0x30000*8+4, 8, 12, 0x30000*8+8, 0x30000*8+12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


static struct GfxLayout klax_molayout =
{
	8,8,	  /* 8*8 sprites */
	4096,   /* 4096 of them */
	4,		  /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0, 4, 0x30000*8+0, 0x30000*8+4, 8, 12, 0x30000*8+8, 0x30000*8+12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every sprite takes 16 consecutive bytes */
};


static struct GfxDecodeInfo klax_gfxdecodeinfo[] =
{
	{ 1, 0x00000, &klax_pflayout,  256, 16 },		/* sprites & playfield */
	{ 1, 0x20000, &klax_molayout,    0, 16 },		/* sprites & playfield */
	{ -1 } /* end of array */
};



/*************************************
 *
 *		Sound definitions
 *
 *************************************/

static struct OKIM6295interface okim6295_interface =
{
	1,			/* 1 chip */
	7159160 / 1024,    /* ~7000 Hz */
	2,       /* memory region 2 */
	{ 255 }
};



/*************************************
 *
 *		Machine driver
 *
 *************************************/

static struct MachineDriver klax_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			7159160,		/* 7.159 Mhz */
			0,
			klax_readmem,klax_writemem,0,0,
			klax_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	klax_init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	klax_gfxdecodeinfo,
	512, 512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_SUPPORTS_DIRTY,
	0,
	klax_vh_start,
	klax_vh_stop,
	klax_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};



/*************************************
 *
 *		ROM definition(s)
 *
 *************************************/

ROM_START( klax_rom )
	ROM_REGION(0x40000)	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "136075-6.006", 0x00000, 0x10000, 0x29c37e65 )
	ROM_LOAD_ODD ( "136075-6.005", 0x00000, 0x10000, 0xe37fb739 )
	ROM_LOAD_EVEN( "136075-6.008", 0x20000, 0x10000, 0x2dd6150a )
	ROM_LOAD_ODD ( "136075-6.007", 0x20000, 0x10000, 0x1493af37 )

	ROM_REGION(0x60000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "136075-2.010", 0x00000, 0x10000, 0x4d657d89 )
	ROM_LOAD( "136075-2.012", 0x10000, 0x10000, 0x649734d3 )
	ROM_LOAD( "136075-2.014", 0x20000, 0x10000, 0x11012575 )
	ROM_LOAD( "136075-2.009", 0x30000, 0x10000, 0xa0254981 )
	ROM_LOAD( "136075-2.011", 0x40000, 0x10000, 0x3def5741 )
	ROM_LOAD( "136075-2.013", 0x50000, 0x10000, 0x09c2a410 )

	ROM_REGION(0x20000)	/* ADPCM data */
	ROM_LOAD( "136075-1.015", 0x00000, 0x10000, 0x98e01950 )
	ROM_LOAD( "136075-1.016", 0x10000, 0x10000, 0x857903b5 )
ROM_END


ROM_START( klaxalt_rom )
	ROM_REGION(0x40000)	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "136075.006", 0x00000, 0x10000, 0x43bd3a2d )
	ROM_LOAD_ODD ( "136075.005", 0x00000, 0x10000, 0xa20aac6c )
	ROM_LOAD_EVEN( "136075.008", 0x20000, 0x10000, 0x86f28500 )
	ROM_LOAD_ODD ( "136075.007", 0x20000, 0x10000, 0xd2b58d41 )

	ROM_REGION(0x60000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "136075.010", 0x00000, 0x10000, 0x4d657d89 )
	ROM_LOAD( "136075.012", 0x10000, 0x10000, 0x649734d3 )
	ROM_LOAD( "136075.014", 0x20000, 0x10000, 0x11012575 )
	ROM_LOAD( "136075.009", 0x30000, 0x10000, 0xa0254981 )
	ROM_LOAD( "136075.011", 0x40000, 0x10000, 0x3def5741 )
	ROM_LOAD( "136075.013", 0x50000, 0x10000, 0x09c2a410 )

	ROM_REGION(0x20000)	/* ADPCM data */
	ROM_LOAD( "136075.015", 0x00000, 0x10000, 0x98e01950 )
	ROM_LOAD( "136075.016", 0x10000, 0x10000, 0x857903b5 )
ROM_END



/*************************************
 *
 *		Game driver(s)
 *
 *************************************/

struct GameDriver klax_driver =
{
	__FILE__,
	0,
	"klax",
	"Klax",
	"????",
	"?????",
	"Aaron Giles (MAME driver)\nMike Cuddy (additional information)",
	0,
	&klax_machine_driver,

	klax_rom,
	0,
	0,
	0,
	0,	/* sound_prom */

	klax_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	atarigen_hiload, atarigen_hisave
};


struct GameDriver klaxalt_driver =
{
	__FILE__,
	0,
	"klaxalt",
	"Klax (Alternate)",
	"????",
	"?????",
	"Aaron Giles (MAME driver)\nMike Cuddy (additional information)",
	0,
	&klax_machine_driver,

	klaxalt_rom,
	0,
	0,
	0,
	0,	/* sound_prom */

	klax_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	atarigen_hiload, atarigen_hisave
};
