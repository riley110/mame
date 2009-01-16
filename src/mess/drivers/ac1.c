/***************************************************************************

        AC1 video driver by Miodrag Milanovic

        15/01/2009 Preliminary driver.

****************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "machine/z80pio.h"
#include "includes/ac1.h"

static GFXDECODE_START( ac1 )
	GFXDECODE_ENTRY( "gfx1", 0x0000, ac1_charlayout, 0, 1 )
GFXDECODE_END

/* Address maps */
static ADDRESS_MAP_START(ac1_mem, ADDRESS_SPACE_PROGRAM, 8)
	ADDRESS_MAP_UNMAP_HIGH
    AM_RANGE( 0x0000, 0x07ff ) AM_ROM  // Monitor
    AM_RANGE( 0x0800, 0x0fff ) AM_ROM  // BASIC
    AM_RANGE( 0x1000, 0x17ff ) AM_RAM  // Video RAM 
    AM_RANGE( 0x1800, 0x1fff ) AM_RAM  // RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( ac1_io, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x04, 0x07) AM_DEVREADWRITE(Z80PIO, "z80pio", z80pio_r, z80pio_w)
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( ac1 )
	PORT_START("LINE0")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0") PORT_CODE(KEYCODE_0)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1") PORT_CODE(KEYCODE_1)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2") PORT_CODE(KEYCODE_2)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3") PORT_CODE(KEYCODE_3)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4") PORT_CODE(KEYCODE_4)
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5") PORT_CODE(KEYCODE_5)
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6") PORT_CODE(KEYCODE_6)
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7") PORT_CODE(KEYCODE_7)
	PORT_START("LINE1")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8") PORT_CODE(KEYCODE_8)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9") PORT_CODE(KEYCODE_9)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(":") PORT_CODE(KEYCODE_QUOTE)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(";") PORT_CODE(KEYCODE_COLON)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("<") PORT_CODE(KEYCODE_COMMA)
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("=") PORT_CODE(KEYCODE_EQUALS)
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(">") PORT_CODE(KEYCODE_STOP)
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("?") PORT_CODE(KEYCODE_SLASH)
	PORT_START("LINE2")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("@") PORT_CODE(KEYCODE_MINUS)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D)
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E)
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F)
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G)
	PORT_START("LINE3")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L)
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M)
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N)
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O)
	PORT_START("LINE4")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T)
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U)
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V)
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W)
	PORT_START("LINE5")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Y)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Z)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_UNUSED)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_UNUSED)
		PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_UNUSED)
		PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_START("LINE6")
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_LSHIFT)   PORT_CODE(KEYCODE_RSHIFT)
		PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Ctrl")  PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(KEYCODE_RCONTROL)
		PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE)
		PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Enter") PORT_CODE(KEYCODE_ENTER)
		PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Backspace") PORT_CODE(KEYCODE_BACKSPACE)		
INPUT_PORTS_END

/* Machine driver */
static MACHINE_DRIVER_START( ac1 )
	/* basic machine hardware */
	MDRV_CPU_ADD("main", Z80, XTAL_8MHz / 4)
	MDRV_CPU_PROGRAM_MAP(ac1_mem, 0)
	MDRV_CPU_IO_MAP(ac1_io, 0)
	MDRV_MACHINE_RESET( ac1 )

	MDRV_Z80PIO_ADD( "z80pio", ac1_z80pio_intf )

	/* video hardware */
	MDRV_SCREEN_ADD("main", RASTER)
	MDRV_SCREEN_REFRESH_RATE(50)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(64*6, 16*8)
	MDRV_SCREEN_VISIBLE_AREA(0, 64*6-1, 0, 16*8-1)
	MDRV_GFXDECODE( ac1 )

	MDRV_PALETTE_LENGTH(2)
	MDRV_PALETTE_INIT(black_and_white)

	MDRV_VIDEO_START(ac1)
	MDRV_VIDEO_UPDATE(ac1)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( ac1_32 )
	MDRV_IMPORT_FROM( ac1 )
	MDRV_SCREEN_MODIFY("main")
	MDRV_SCREEN_SIZE(64*6, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(0, 64*6-1, 0, 32*8-1)
	MDRV_VIDEO_UPDATE(ac1_32)
MACHINE_DRIVER_END
	
/* ROM definition */
ROM_START( ac1 )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "mon_v31_16.bin",  0x0000, 0x0800, CRC(1ba65e4d) SHA1(3382b8d03f31166a56aea49fd1ec1e82a7108300))
	ROM_LOAD( "minibasic.bin",   0x0800, 0x0800, CRC(06782639) SHA1(3fd57b3ae3f538374b0d794d8aa15d06bcaaddd8))
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("zg_128.bin", 0x0000, 0x0400, CRC(0a6f7796) SHA1(64d77639b1ea23f45b4bd38c251851acb2d03822))
	ROM_FILL( 0x0400, 0x0400, 0xFF )
ROM_END

ROM_START( ac1_32 )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "mon_v31_32.bin",  0x0000, 0x0800, CRC(bea78b1a) SHA1(8a3e2ac2033aa0bb016be742cfea7e4b09c0813b))
	ROM_LOAD( "minibasic.bin",   0x0800, 0x0800, CRC(06782639) SHA1(3fd57b3ae3f538374b0d794d8aa15d06bcaaddd8))
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("zg_256.bin", 0x0000, 0x0800, CRC(b4171df5) SHA1(abdec4e00257f86b1a57e02b9c6b4d2df2a2a2db))
ROM_END

/* Driver */

/*    YEAR  NAME   PARENT  COMPAT  MACHINE  INPUT   INIT  CONFIG COMPANY                 FULLNAME   FLAGS */
COMP( 1984, ac1,     0,      0, 	ac1, 	ac1, 	ac1, NULL,  "Frank Heyder", "Amateurcomputer AC1",		 0)
COMP( 1984, ac1_32,  ac1,    0, 	ac1_32,	ac1, 	ac1, NULL,  "Frank Heyder", "Amateurcomputer AC1 (32 lines)",		 0)
