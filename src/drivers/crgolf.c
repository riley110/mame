/***************************************************************************

	Kitco Crowns Golf hardware

	driver by Aaron Giles

	Games supported:
		* Crowns Golf (3 sets)

	Known bugs:
		* not sure if the analog inputs are handled correctly

****************************************************************************

	Memory map (TBA)

***************************************************************************/

#include "driver.h"
#include "crgolf.h"


/* constants */
#define MASTER_CLOCK		18432000


/* local variables */
static data8_t port_select;
static data8_t main_to_sound_data, sound_to_main_data;



/*************************************
 *
 *	ROM banking
 *
 *************************************/

static WRITE_HANDLER( rom_bank_select_w )
{
	UINT8 *region_base = memory_region(REGION_CPU1);
	cpu_setbank(1, region_base + 0x10000 + (data & 15) * 0x2000);
}


static MACHINE_INIT( crgolf )
{
	rom_bank_select_w(0, 0);
}



/*************************************
 *
 *	Input ports
 *
 *************************************/

static READ_HANDLER( switch_input_r )
{
	return readinputport(port_select);
}


static READ_HANDLER( analog_input_r )
{
	return ((readinputport(7) >> 4) | (readinputport(8) & 0xf0)) ^ 0x88;
}


static WRITE_HANDLER( switch_input_select_w )
{
	if (!(data & 0x40)) port_select = 6;
	if (!(data & 0x20)) port_select = 5;
	if (!(data & 0x10)) port_select = 4;
	if (!(data & 0x08)) port_select = 3;
	if (!(data & 0x04)) port_select = 2;
	if (!(data & 0x02)) port_select = 1;
	if (!(data & 0x01)) port_select = 0;
}


static WRITE_HANDLER( unknown_w )
{
	logerror("%04X:unknown_w = %02X\n", activecpu_get_pc(), data);
}



/*************************************
 *
 *	Main->Sound CPU communications
 *
 *************************************/

static void main_to_sound_callback(int param)
{
	cpu_set_irq_line(1, IRQ_LINE_NMI, ASSERT_LINE);
	main_to_sound_data = param;
}


static WRITE_HANDLER( main_to_sound_w )
{
	timer_set(TIME_NOW, data, main_to_sound_callback);
}


static READ_HANDLER( main_to_sound_r )
{
	cpu_set_irq_line(1, IRQ_LINE_NMI, CLEAR_LINE);
	return main_to_sound_data;
}



/*************************************
 *
 *	Sound->Main CPU communications
 *
 *************************************/

static void sound_to_main_callback(int param)
{
	cpu_set_irq_line(0, IRQ_LINE_NMI, ASSERT_LINE);
	sound_to_main_data = param;
}


static WRITE_HANDLER( sound_to_main_w )
{
	timer_set(TIME_NOW, data, sound_to_main_callback);
}


static READ_HANDLER( sound_to_main_r )
{
	cpu_set_irq_line(0, IRQ_LINE_NMI, CLEAR_LINE);
	return sound_to_main_data;
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static MEMORY_READ_START( main_readmem )
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x5fff, MRA_RAM },
	{ 0x6000, 0x7fff, MRA_BANK1 },
	{ 0x8800, 0x8800, sound_to_main_r },
	{ 0xa000, 0xbfff, crgolf_videoram_bit1_r },
	{ 0xc000, 0xdfff, crgolf_videoram_bit0_r },
	{ 0xe000, 0xffff, crgolf_videoram_bit2_r },
MEMORY_END


static MEMORY_WRITE_START( main_writemem )
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x5fff, MWA_RAM },
	{ 0x6000, 0x7fff, MWA_ROM },
	{ 0x8003, 0x8003, MWA_RAM, &crgolf_color_select },
	{ 0x8004, 0x8004, MWA_RAM, &crgolf_screen_flip },
	{ 0x8005, 0x8005, MWA_RAM, &crgolf_screen_select },
	{ 0x8006, 0x8006, MWA_RAM, &crgolf_screenb_enable },
	{ 0x8007, 0x8007, MWA_RAM, &crgolf_screena_enable },
	{ 0x8800, 0x8800, main_to_sound_w },
	{ 0x9000, 0x9000, rom_bank_select_w },
	{ 0xa000, 0xbfff, crgolf_videoram_bit1_w },
	{ 0xc000, 0xdfff, crgolf_videoram_bit0_w },
	{ 0xe000, 0xffff, crgolf_videoram_bit2_w },
MEMORY_END



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0xe000, 0xe000, switch_input_r },
	{ 0xe001, 0xe001, analog_input_r },
	{ 0xe003, 0xe003, main_to_sound_r },
MEMORY_END


static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0xc000, 0xc000, AY8910_control_port_0_w },
	{ 0xc001, 0xc001, AY8910_write_port_0_w },
	{ 0xc002, 0xc002, MWA_NOP },
	{ 0xe000, 0xe000, switch_input_select_w },
	{ 0xe001, 0xe001, unknown_w },
	{ 0xe003, 0xe003, sound_to_main_w },
MEMORY_END



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( crgolf )
	PORT_START	/* CREDIT */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0xfc, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* SELECT */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START4 )
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* PLAY1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON6 | IPF_PLAYER1 )			/* club select */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )			/* backward address */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER1 )			/* forward address */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 | IPF_PLAYER1 )			/* open stance */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON5 | IPF_PLAYER1 )			/* closed stance */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )	/* direction left */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )	/* direction right */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )			/* shot switch */

	PORT_START	/* PLAY2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON6 | IPF_COCKTAIL )		/* club select */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_COCKTAIL )		/* backward address */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_COCKTAIL )		/* forward address */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 | IPF_COCKTAIL )		/* open stance */
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON5 | IPF_COCKTAIL )		/* closed stance */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_COCKTAIL )	/* direction left */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_COCKTAIL )	/* direction right */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )		/* shot switch */

	PORT_START	/* DIPSW */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Difficulty ))
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPNAME( 0x06, 0x04, "Half-Round Play" )
	PORT_DIPSETTING(    0x00, "4 Coins" )
	PORT_DIPSETTING(    0x02, "5 Coins" )
	PORT_DIPSETTING(    0x04, "6 Coins" )
	PORT_DIPSETTING(    0x06, "10 Coins" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Cabinet ))
	PORT_DIPSETTING(    0x08, DEF_STR( Upright ))
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ))
	PORT_DIPNAME( 0x10, 0x00, "Clear High Scores" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x10, DEF_STR( On ))
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Coinage ))
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ))
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_REVERSE | IPF_CENTER, 70, 16, 0, 255 )

	PORT_START
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_REVERSE | IPF_CENTER | IPF_COCKTAIL, 70, 16, 0, 255 )
INPUT_PORTS_END



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct AY8910interface ay8910_interface =
{
	1,
	MASTER_CLOCK/3/2/2,
	{ 100 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static MACHINE_DRIVER_START( crgolf )

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80,MASTER_CLOCK/3/2)
	MDRV_CPU_MEMORY(main_readmem,main_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_CPU_ADD(Z80,MASTER_CLOCK/3/2)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_VBLANK_INT(irq0_line_hold,1)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(crgolf)
	MDRV_INTERLEAVE(100)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_VISIBLE_AREA(0, 255, 8, 247)
	MDRV_PALETTE_LENGTH(32)

	MDRV_PALETTE_INIT(crgolf)
	MDRV_VIDEO_START(crgolf)
	MDRV_VIDEO_UPDATE(crgolf)

	/* sound hardware */
	MDRV_SOUND_ADD(AY8910, ay8910_interface)
MACHINE_DRIVER_END



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( crgolf )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "epr-6143.1c",0x00000, 0x2000, 0x4b301360 )
	ROM_LOAD( "epr-6142.1a",0x02000, 0x2000, 0x8fc5e67f )
	ROM_LOAD( "crnsgolf.c", 0x10000, 0x2000, 0x4d6d8dad )
	ROM_LOAD( "crnsgolf.h", 0x1e000, 0x2000, 0xfac6d56c )
	ROM_LOAD( "crnsgolf.d", 0x20000, 0x2000, 0xdd48dc1f )
	ROM_LOAD( "crnsgolf.i", 0x22000, 0x2000, 0xa09b27b8 )
	ROM_LOAD( "crnsgolf.e", 0x24000, 0x2000, 0xfb86a168 )
	ROM_LOAD( "crnsgolf.j", 0x26000, 0x2000, 0x981f03ef )
	ROM_LOAD( "crnsgolf.f", 0x28000, 0x2000, 0xe64125ff )
	ROM_LOAD( "crnsgolf.k", 0x2a000, 0x2000, 0xefc0e15a )
	ROM_LOAD( "crnsgolf.g", 0x2c000, 0x2000, 0xeb455966 )
	ROM_LOAD( "crnsgolf.l", 0x2e000, 0x2000, 0x88357391 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "epr-6144.f1", 0x0000, 0x2000, 0xb677f818 )
	ROM_LOAD( "5892",        0x2000, 0x2000, 0x608dc2e2 )
	ROM_LOAD( "5891a",       0x4000, 0x2000, 0xf353b585 )
	ROM_LOAD( "5890b",       0x6000, 0x2000, 0xb737c2e8 )

	ROM_REGION( 0x0020,  REGION_PROMS, 0 )
	ROM_LOAD( "golfprom", 0x0000, 0x0020, 0xf880b95d )
ROM_END


ROM_START( crgolfa )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "5879b",      0x00000, 0x2000, 0x927be359 )
	ROM_LOAD( "5878",       0x02000, 0x2000, 0x65fd0fa0 )
	ROM_LOAD( "crnsgolf.c", 0x10000, 0x2000, 0x4d6d8dad )
	ROM_LOAD( "crnsgolf.h", 0x1e000, 0x2000, 0xfac6d56c )
	ROM_LOAD( "crnsgolf.d", 0x20000, 0x2000, 0xdd48dc1f )
	ROM_LOAD( "crnsgolf.i", 0x22000, 0x2000, 0xa09b27b8 )
	ROM_LOAD( "crnsgolf.e", 0x24000, 0x2000, 0xfb86a168 )
	ROM_LOAD( "crnsgolf.j", 0x26000, 0x2000, 0x981f03ef )
	ROM_LOAD( "crnsgolf.f", 0x28000, 0x2000, 0xe64125ff )
	ROM_LOAD( "crnsgolf.k", 0x2a000, 0x2000, 0xefc0e15a )
	ROM_LOAD( "crnsgolf.g", 0x2c000, 0x2000, 0xeb455966 )
	ROM_LOAD( "crnsgolf.l", 0x2e000, 0x2000, 0x88357391 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "5893c",  0x0000, 0x1000, 0x5011646d )
	ROM_LOAD( "5892",   0x2000, 0x2000, 0x608dc2e2 )
	ROM_LOAD( "5891a",  0x4000, 0x2000, 0xf353b585 )
	ROM_LOAD( "5890b",  0x6000, 0x2000, 0xb737c2e8 )

	ROM_REGION( 0x0020,  REGION_PROMS, 0 )
	ROM_LOAD( "golfprom", 0x0000, 0x0020, 0xf880b95d )
ROM_END


ROM_START( crgolfb )
	ROM_REGION( 0x30000, REGION_CPU1, 0 )
	ROM_LOAD( "5879b",      0x00000, 0x2000, 0x927be359 )
	ROM_LOAD( "5878",       0x02000, 0x2000, 0x65fd0fa0 )
	ROM_LOAD( "cg.1",       0x10000, 0x2000, 0xad7d537a )
	ROM_LOAD( "crnsgolf.h", 0x1e000, 0x2000, 0xfac6d56c )
	ROM_LOAD( "crnsgolf.d", 0x20000, 0x2000, 0xdd48dc1f )
	ROM_LOAD( "crnsgolf.i", 0x22000, 0x2000, 0xa09b27b8 )
	ROM_LOAD( "crnsgolf.e", 0x24000, 0x2000, 0xfb86a168 )
	ROM_LOAD( "crnsgolf.j", 0x26000, 0x2000, 0x981f03ef )
	ROM_LOAD( "crnsgolf.f", 0x28000, 0x2000, 0xe64125ff )
	ROM_LOAD( "crnsgolf.k", 0x2a000, 0x2000, 0xefc0e15a )
	ROM_LOAD( "crnsgolf.g", 0x2c000, 0x2000, 0xeb455966 )
	ROM_LOAD( "crnsgolf.l", 0x2e000, 0x2000, 0x88357391 )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "cg.14",  0x0000, 0x1000, 0x07156cd9 )
	ROM_LOAD( "5892",   0x2000, 0x2000, 0x608dc2e2 )
	ROM_LOAD( "5891a",  0x4000, 0x2000, 0xf353b585 )
	ROM_LOAD( "5890b",  0x6000, 0x2000, 0xb737c2e8 )

	ROM_REGION( 0x0020,  REGION_PROMS, 0 )
	ROM_LOAD( "golfprom", 0x0000, 0x0020, 0xf880b95d )
ROM_END



/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1984, crgolf,  0,      crgolf,  crgolf,  0, ROT0, "Nasco Japan", "Crowns Golf (set 1)" )
GAME( 1984, crgolfa, crgolf, crgolf,  crgolf,  0, ROT0, "Nasco Japan", "Crowns Golf (set 2)" )
GAME( 1984, crgolfb, crgolf, crgolf,  crgolf,  0, ROT0, "Nasco Japan", "Champion Golf (bootleg)" )
