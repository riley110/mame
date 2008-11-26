/***************************************************************************

        Radio-86RK driver by Miodrag Milanovic

        15/03/2008 Preliminary driver.

****************************************************************************/


#include "driver.h"
#include "cpu/i8085/i8085.h"
#include "machine/8255ppi.h"
#include "machine/8257dma.h"
#include "video/i8275.h"
#include "devices/cassette.h"
#include "formats/rk_cas.h"
#include "includes/radio86.h"

/* Address maps */
static ADDRESS_MAP_START(radio86_mem, ADDRESS_SPACE_PROGRAM, 8)
    AM_RANGE( 0x0000, 0x0fff ) AM_RAMBANK(1) // First bank
    AM_RANGE( 0x1000, 0x7fff ) AM_RAM  // RAM
    AM_RANGE( 0x8000, 0x8003 ) AM_DEVREADWRITE(PPI8255, "ppi8255_1", ppi8255_r, ppi8255_w) AM_MIRROR(0x1ffc)
    //AM_RANGE( 0xa000, 0xa003 ) AM_DEVREADWRITE(PPI8255, "ppi8255_2", ppi8255_r, ppi8255_w) AM_MIRROR(0x1ffc)
    AM_RANGE( 0xc000, 0xc001 ) AM_DEVREADWRITE(I8275, "i8275", i8275_r, i8275_w) AM_MIRROR(0x1ffe) // video
    AM_RANGE( 0xe000, 0xffff ) AM_DEVWRITE(DMA8257, "dma8257", dma8257_w)	 // DMA
    AM_RANGE( 0xf000, 0xffff ) AM_ROM  // System ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( radio86_io , ADDRESS_SPACE_IO, 8)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE( 0x00, 0xff ) AM_READWRITE(radio_io_r,radio_io_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( rk7007_io , ADDRESS_SPACE_IO, 8)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE( 0x80, 0x83 ) AM_DEVREADWRITE(PPI8255, "ms7007", ppi8255_r, ppi8255_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START(radio86rom_mem, ADDRESS_SPACE_PROGRAM, 8)
    AM_RANGE( 0x0000, 0x0fff ) AM_RAMBANK(1) // First bank
    AM_RANGE( 0x1000, 0x7fff ) AM_RAM  // RAM
    AM_RANGE( 0x8000, 0x8003 ) AM_DEVREADWRITE(PPI8255, "ppi8255_1", ppi8255_r, ppi8255_w) AM_MIRROR(0x1ffc)
    AM_RANGE( 0xa000, 0xa003 ) AM_DEVREADWRITE(PPI8255, "ppi8255_2", ppi8255_r, ppi8255_w) AM_MIRROR(0x1ffc)
    AM_RANGE( 0xc000, 0xc001 ) AM_DEVREADWRITE(I8275, "i8275", i8275_r, i8275_w) AM_MIRROR(0x1ffe) // video
    AM_RANGE( 0xe000, 0xffff ) AM_DEVWRITE(DMA8257, "dma8257", dma8257_w)	 // DMA
    AM_RANGE( 0xf000, 0xffff ) AM_ROM  // System ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START(radio86ram_mem, ADDRESS_SPACE_PROGRAM, 8)
    AM_RANGE( 0x0000, 0x0fff ) AM_RAMBANK(1) // First bank
    AM_RANGE( 0x1000, 0xdfff ) AM_RAM  // RAM
    AM_RANGE( 0xe000, 0xe7ff ) AM_ROM  // System ROM page 2
    AM_RANGE( 0xe800, 0xf5ff ) AM_RAM  // RAM
    AM_RANGE( 0xf700, 0xf703 ) AM_DEVREADWRITE(PPI8255, "ppi8255_1", ppi8255_r, ppi8255_w)
    AM_RANGE( 0xf780, 0xf7bf ) AM_DEVREADWRITE(I8275, "i8275", i8275_r, i8275_w) // video
    AM_RANGE( 0xf684, 0xf687 ) AM_DEVREADWRITE(PPI8255, "ppi8255_2", ppi8255_r, ppi8255_w)
	  AM_RANGE( 0xf688, 0xf688 ) AM_WRITE( radio86_pagesel )
    AM_RANGE( 0xf800, 0xffff ) AM_DEVWRITE(DMA8257, "dma8257", dma8257_w)	 // DMA
    AM_RANGE( 0xf800, 0xffff ) AM_ROM  // System ROM page 1
ADDRESS_MAP_END

static ADDRESS_MAP_START(radio86_16_mem, ADDRESS_SPACE_PROGRAM, 8)
	ADDRESS_MAP_UNMAP_HIGH
    AM_RANGE( 0x0000, 0x0fff ) AM_RAMBANK(1) // First bank
    AM_RANGE( 0x1000, 0x3fff ) AM_RAM  // RAM
    AM_RANGE( 0x4000, 0x7fff ) AM_READ(radio_cpu_state_r)
    AM_RANGE( 0x8000, 0x8003 ) AM_DEVREADWRITE(PPI8255, "ppi8255_1", ppi8255_r, ppi8255_w) AM_MIRROR(0x1ffc)
    //AM_RANGE( 0xa000, 0xa003 ) AM_DEVREADWRITE(PPI8255, "ppi8255_2", ppi8255_r, ppi8255_w) AM_MIRROR(0x1ffc)
    AM_RANGE( 0xc000, 0xc001 ) AM_DEVREADWRITE(I8275, "i8275", i8275_r, i8275_w) AM_MIRROR(0x1ffe) // video
    AM_RANGE( 0xe000, 0xffff ) AM_DEVWRITE(DMA8257, "dma8257", dma8257_w)	 // DMA
    AM_RANGE( 0xf000, 0xffff ) AM_ROM  // System ROM
ADDRESS_MAP_END

/* Input ports */
INPUT_PORTS_START( radio86 )
	PORT_START("LINE0")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Home") PORT_CODE(KEYCODE_HOME)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Ins") PORT_CODE(KEYCODE_INSERT)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Esc") PORT_CODE(KEYCODE_ESC)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F1") PORT_CODE(KEYCODE_F1)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F2") PORT_CODE(KEYCODE_F2)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F3") PORT_CODE(KEYCODE_F3)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F4") PORT_CODE(KEYCODE_F4)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F5") PORT_CODE(KEYCODE_F5)
	PORT_START("LINE1")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Tab") PORT_CODE(KEYCODE_TAB)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("End") PORT_CODE(KEYCODE_END)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Enter") PORT_CODE(KEYCODE_ENTER)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Back") PORT_CODE(KEYCODE_BACKSPACE)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Left") PORT_CODE(KEYCODE_LEFT)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Up") PORT_CODE(KEYCODE_UP)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Right") PORT_CODE(KEYCODE_RIGHT)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Down") PORT_CODE(KEYCODE_DOWN)
	PORT_START("LINE2")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0") PORT_CODE(KEYCODE_0)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1") PORT_CODE(KEYCODE_1)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2") PORT_CODE(KEYCODE_2)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3") PORT_CODE(KEYCODE_3)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4") PORT_CODE(KEYCODE_4)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5") PORT_CODE(KEYCODE_5)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6") PORT_CODE(KEYCODE_6)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7") PORT_CODE(KEYCODE_7)
	PORT_START("LINE3")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8") PORT_CODE(KEYCODE_8)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9") PORT_CODE(KEYCODE_9)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(":") PORT_CODE(KEYCODE_EQUALS)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("+") PORT_CODE(KEYCODE_COLON)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("<") PORT_CODE(KEYCODE_COMMA)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("-") PORT_CODE(KEYCODE_MINUS)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(">") PORT_CODE(KEYCODE_STOP)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("?") PORT_CODE(KEYCODE_SLASH)
	PORT_START("LINE4")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("~") PORT_CODE(KEYCODE_TILDE)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G)
	PORT_START("LINE5")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O)
	PORT_START("LINE6")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W)
	PORT_START("LINE7")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Y)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Z)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("[") PORT_CODE(KEYCODE_OPENBRACE)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("\\") PORT_CODE(KEYCODE_BACKSLASH)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("]") PORT_CODE(KEYCODE_CLOSEBRACE)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("'") PORT_CODE(KEYCODE_QUOTE)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE)
	PORT_START("LINE8")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_LSHIFT) PORT_CODE(KEYCODE_RSHIFT)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Ctrl") PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(KEYCODE_RCONTROL)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Rus/Lat") PORT_CODE(KEYCODE_LALT) PORT_CODE(KEYCODE_RALT)
INPUT_PORTS_END

INPUT_PORTS_START( ms7007 )
	PORT_START("LINE0")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0") PORT_CODE(KEYCODE_0)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9") PORT_CODE(KEYCODE_9)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 6") PORT_CODE(KEYCODE_6_PAD)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Ins") PORT_CODE(KEYCODE_INSERT)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("End") PORT_CODE(KEYCODE_END)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("?") PORT_CODE(KEYCODE_SLASH)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("-") PORT_CODE(KEYCODE_MINUS)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(":") PORT_CODE(KEYCODE_EQUALS)
	PORT_START("LINE1")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7") PORT_CODE(KEYCODE_7)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8") PORT_CODE(KEYCODE_8)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F1") PORT_CODE(KEYCODE_F1)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F2") PORT_CODE(KEYCODE_F2)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F3") PORT_CODE(KEYCODE_F3)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F4") PORT_CODE(KEYCODE_F4)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F5") PORT_CODE(KEYCODE_F5)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4") PORT_CODE(KEYCODE_4)
	PORT_START("LINE2")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("[") PORT_CODE(KEYCODE_OPENBRACE)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("]") PORT_CODE(KEYCODE_CLOSEBRACE)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1") PORT_CODE(KEYCODE_1)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2") PORT_CODE(KEYCODE_2)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3") PORT_CODE(KEYCODE_3)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5") PORT_CODE(KEYCODE_5)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6") PORT_CODE(KEYCODE_6)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E)
	PORT_START("LINE3")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P)
	PORT_START("LINE4")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("@") PORT_CODE(KEYCODE_TILDE)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Y)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I)
	PORT_START("LINE5")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Left") PORT_CODE(KEYCODE_LEFT)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("<") PORT_CODE(KEYCODE_COMMA)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("'") PORT_CODE(KEYCODE_QUOTE)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE)
	PORT_START("LINE6")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("\\") PORT_CODE(KEYCODE_BACKSLASH)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 4") PORT_CODE(KEYCODE_4_PAD)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num +") PORT_CODE(KEYCODE_PLUS_PAD)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Back") PORT_CODE(KEYCODE_BACKSPACE)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Down") PORT_CODE(KEYCODE_DOWN)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(">") PORT_CODE(KEYCODE_STOP)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Right") PORT_CODE(KEYCODE_RIGHT)
	PORT_START("LINE7")
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H)
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Z)
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 5") PORT_CODE(KEYCODE_5_PAD)
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F6") PORT_CODE(KEYCODE_F6)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Home") PORT_CODE(KEYCODE_HOME)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Up") PORT_CODE(KEYCODE_UP)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num /") PORT_CODE(KEYCODE_SLASH_PAD)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Enter") PORT_CODE(KEYCODE_ENTER)	
	PORT_START("LINE8")
		PORT_BIT(0xFF, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_START("CLINE0")
		PORT_BIT(0x1F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 9") PORT_CODE(KEYCODE_9_PAD)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num Enter") PORT_CODE(KEYCODE_ENTER_PAD)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 3") PORT_CODE(KEYCODE_3_PAD)		
	PORT_START("CLINE1")
		PORT_BIT(0x1F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num *") PORT_CODE(KEYCODE_ASTERISK)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Esc") PORT_CODE(KEYCODE_ESC) 
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num +") PORT_CODE(KEYCODE_PLUS_PAD)
	PORT_START("CLINE2")
		PORT_BIT(0x1F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num -") PORT_CODE(KEYCODE_MINUS_PAD)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Tab") PORT_CODE(KEYCODE_TAB)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J)		
	PORT_START("CLINE3")
		PORT_BIT(0x7F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F) 
	PORT_START("CLINE4")
		PORT_BIT(0x7F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q) 
	PORT_START("CLINE5")
		PORT_BIT(0x1F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_LSHIFT)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Ctrl") PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(KEYCODE_RCONTROL)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Rus/Lat") PORT_CODE(KEYCODE_LALT) PORT_CODE(KEYCODE_RALT)
	PORT_START("CLINE6")
		PORT_BIT(0x1F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 7") PORT_CODE(KEYCODE_7_PAD)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 0") PORT_CODE(KEYCODE_0_PAD)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 1") PORT_CODE(KEYCODE_1_PAD)		
	PORT_START("CLINE7")
		PORT_BIT(0x1F, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 8") PORT_CODE(KEYCODE_8_PAD)
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num .") PORT_CODE(KEYCODE_DEL_PAD)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Num 2") PORT_CODE(KEYCODE_2_PAD)
INPUT_PORTS_END

static const cassette_config radio86_cassette_config =
{
	rkr_cassette_formats,
	NULL,
	CASSETTE_STOPPED | CASSETTE_SPEAKER_ENABLED | CASSETTE_MOTOR_ENABLED
};


/* Machine driver */
static MACHINE_DRIVER_START( radio86 )
  /* basic machine hardware */
  MDRV_CPU_ADD("main",8080, XTAL_16MHz / 9)
  MDRV_CPU_PROGRAM_MAP(radio86_mem, 0)
  MDRV_CPU_IO_MAP(radio86_io, 0)
  MDRV_MACHINE_RESET( radio86 )

	MDRV_DEVICE_ADD( "ppi8255_1", PPI8255 )
	MDRV_DEVICE_CONFIG( radio86_ppi8255_interface_1 )

	MDRV_DEVICE_ADD( "i8275", I8275 )
	MDRV_DEVICE_CONFIG(radio86_i8275_interface)
    /* video hardware */
	MDRV_SCREEN_ADD("main", RASTER)
	MDRV_SCREEN_REFRESH_RATE(50)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(78*6, 30*10)
	MDRV_SCREEN_VISIBLE_AREA(0, 78*6-1, 0, 30*10-1)
	MDRV_PALETTE_LENGTH(3)
	MDRV_PALETTE_INIT(radio86)

	MDRV_VIDEO_START(generic_bitmapped)
	MDRV_VIDEO_UPDATE(radio86)
	
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("cassette", WAVE, 0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)

	MDRV_DEVICE_ADD("dma8257", DMA8257)
	MDRV_DEVICE_CONFIG(radio86_dma)

	MDRV_CASSETTE_ADD( "cassette", radio86_cassette_config )
MACHINE_DRIVER_END

static UINT8 *radio16_io_mirror = NULL;

static DIRECT_UPDATE_HANDLER( radio16_direct )
{	
	if (address >= 0x4000 && address <=0x7FFF) {
			direct->mask = 0xffff;
			direct->raw = radio16_io_mirror;
			direct->decrypted = radio16_io_mirror;
			direct->min = 0x4000;
			direct->max = 0x7fff;
			radio16_io_mirror[address] = cpu_get_reg(space->machine->cpu[0], I8080_STATUS);
	} 
	return address;
}

static MACHINE_START( radio16 )
{
	radio16_io_mirror = auto_malloc( 0x8000 );
	memory_set_direct_update_handler( cpu_get_address_space(machine->cpu[0], ADDRESS_SPACE_PROGRAM), radio16_direct );
}

static MACHINE_DRIVER_START( radio16 )
  /* basic machine hardware */
  MDRV_IMPORT_FROM(radio86)
  MDRV_CPU_MODIFY("main")
  MDRV_CPU_PROGRAM_MAP(radio86_16_mem, 0)
  MDRV_MACHINE_START( radio16 )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( radiorom )
  /* basic machine hardware */
  MDRV_IMPORT_FROM(radio86)
  MDRV_CPU_MODIFY("main")
  MDRV_CPU_PROGRAM_MAP(radio86rom_mem, 0)
    
	MDRV_DEVICE_ADD( "ppi8255_2", PPI8255 )
	MDRV_DEVICE_CONFIG( radio86_ppi8255_interface_2 )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( radioram )
  /* basic machine hardware */
  MDRV_IMPORT_FROM(radio86)
  MDRV_CPU_MODIFY("main")
  MDRV_CPU_PROGRAM_MAP(radio86ram_mem, 0)

	MDRV_DEVICE_ADD( "ppi8255_2", PPI8255 )
	MDRV_DEVICE_CONFIG( radio86_ppi8255_interface_2 )
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( rk7007 )
  /* basic machine hardware */
  MDRV_IMPORT_FROM(radio86)
  MDRV_CPU_MODIFY("main")
  MDRV_CPU_IO_MAP(rk7007_io, 0)

	MDRV_DEVICE_ADD( "ms7007", PPI8255 )
	MDRV_DEVICE_CONFIG( rk7007_ppi8255_interface )
MACHINE_DRIVER_END
  
static MACHINE_DRIVER_START( rk700716 )
  /* basic machine hardware */
  MDRV_IMPORT_FROM(radio16)
  MDRV_CPU_MODIFY("main")
  MDRV_CPU_IO_MAP(rk7007_io, 0)
    
	MDRV_DEVICE_ADD( "ms7007", PPI8255 )
	MDRV_DEVICE_CONFIG( rk7007_ppi8255_interface )    
MACHINE_DRIVER_END


/* ROM definition */
ROM_START( radio86 )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "bios.rom", 0xf800, 0x0800, CRC(bf1ceea5) SHA1(8f3d472203e550e9854dd79e1f44628635581ed0))
	ROM_COPY( "main", 0xf800, 0xf000, 0x0800 )
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END

ROM_START( radio4k )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "bios4k.rom", 0xf000, 0x1000, CRC(2ac9d864) SHA1(296716c6cddc9dd31d500ba421aa807c45757cfd))
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END

ROM_START( spektr01 )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )	
	ROM_LOAD( "spektr001.rom", 0xf800, 0x0800, CRC(5a38e6d5) SHA1(799c3bbe2a9f08f3aba55379cc093329048350ff))
	ROM_COPY( "main", 0xf800, 0xf000, 0x0800 )
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END

ROM_START( radio16 )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "rk86.16k", 0xf800, 0x0800, CRC(fd8a4caf) SHA1(90d6af571049a7c8748eac03541e921eac3f70c5))
	ROM_COPY( "main", 0xf800, 0xf000, 0x0800 )
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END

ROM_START( radiorom )
	ROM_REGION( 0x18000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "radiorom.rom", 0xf800, 0x0800, CRC(B5CDEAB7) SHA1(1c80d72082f2fb2190b575726cb82d86ae0ee7d8))
	ROM_COPY( "main", 0xf800, 0xf000, 0x0800 )
	ROM_LOAD( "romdisk.rk", 0x10000, 0x8000, CRC(6B16FC04) SHA1(8c09322ae184f4d900f1032d20b5cf3eb2f1a24b))
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END

ROM_START( radioram )
	ROM_REGION( 0x20000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "r86-1.bin", 0xf800, 0x0800, CRC(7E7AB7CB) SHA1(fedb00b6b8fbe1167faba3e4611b483f800e6934))
	ROM_LOAD( "r86-2.bin", 0xe000, 0x0800, CRC(955F0616) SHA1(d2b9f960558bdcb60074091fc79d1ad56c313586))
	ROM_LOAD( "romdisk.bin", 0x10000, 0x10000, CRC(43C0279B) SHA1(bc1dfd9bdbce39460616e2158f5d96279d0af3cf))
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END

ROM_START( rk7007 )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "ms7007.rom", 0xf800, 0x0800, CRC(002811DC) SHA1(4529eb72198c49af77fbcd7833bcd06a1cf9b1ac))
	ROM_COPY( "main", 0xf800, 0xf000, 0x0800 )
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END

ROM_START( rk700716 )
	ROM_REGION( 0x10000, "main", ROMREGION_ERASEFF )
	ROM_LOAD( "ms7007.16k", 0xf800, 0x0800, CRC(5268D7B6) SHA1(efd69d8456b8cf8b37f33237153c659725608528))
	ROM_COPY( "main", 0xf800, 0xf000, 0x0800 )
	ROM_REGION(0x0800, "gfx1",0)
	ROM_LOAD ("radio86.fnt", 0x0000, 0x0400, CRC(7666bd5e) SHA1(8652787603bee9b4da204745e3b2aa07a4783dfc))
ROM_END


/* Driver */

/*    YEAR  NAME    PARENT  COMPAT   MACHINE    INPUT   INIT    CONFIG COMPANY   FULLNAME       FLAGS */
COMP( 1986, radio86, 0,       0, 	radio86, 	radio86,radio86, 0,  "", 	"Radio-86RK",	0)
COMP( 1986, radio16, radio86, 0, 	radio16, 	radio86,radio86, 0,  "", 	"Radio-86RK (16K RAM)",	0)
COMP( 1986, radio4k, radio86, 0, 	radio86, 	radio86,radio86, 0,  "", 	"Radio-86RK (4K ROM)",	0)
COMP( 1986, radiorom,radio86, 0, 	radiorom, 	radio86,radio86, 0,  "", 	"Radio-86RK (ROM-Disk)",	0)
COMP( 1986, radioram, radio86, 0, 	radioram, 	radio86,radioram, 0,  "", "Radio-86RK (ROM/RAM Disk)",	0)
COMP( 1986, spektr01,radio86, 0, 	radio86, 	radio86,radio86, 0,  "", 	"Spektr-001",	0)
COMP( 1986, rk7007, radio86, 0, 	rk7007, 	ms7007,radio86, 0,  "", 	"Radio-86RK (MS7007)",	0)
COMP( 1986, rk700716, radio86, 0, 	rk700716, 	ms7007,radio86, 0,  "", 	"Radio-86RK (MS7007 16K RAM)",	0)
