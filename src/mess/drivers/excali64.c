// license:MAME
// copyright-holders:Robbbert
/***************************************************************************

Excalibur 64 kit computer, designed and sold in Australia by BGR Computers.

Skeleton driver created on 2014-12-09.

Chips: Z80A, 8251, 8253, 8255, 6845
We have Basic 1.1. Other known versions are 1.01, 2.1

Control W then Enter will switch between 40 and 80 characters per line.

ToDo:
- Some keys can be connected to more than one position in the matrix. Need to
  determine the correct positions.
- The position of the "Line Insert" key is unknown.
- PCGEN command not working.
- Colours are wrong (colour prom needs to be dumped)
- Disk controller
- Banking
- The schematic shows the audio counter connected to 2MHz, but this produces
  sounds that are too high. Connected to 1MHz for now.
- Serial
- Parallel / Centronics
- Need software
- Pasting can drop a character or two at the start of a line.
- Clock change for crtc

****************************************************************************/

#include "emu.h"
#include "cpu/z80/z80.h"
#include "video/mc6845.h"
#include "machine/i8251.h"
#include "bus/rs232/rs232.h"
//#include "machine/clock.h"
#include "machine/pit8253.h"
#include "machine/i8255.h"
//#include "bus/centronics/ctronics.h"
#include "imagedev/cassette.h"
#include "sound/wave.h"
#include "sound/speaker.h"


class excali64_state : public driver_device
{
public:
	excali64_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_palette(*this, "palette")
		, m_maincpu(*this, "maincpu")
		, m_cass(*this, "cassette")
		, m_crtc(*this, "crtc")
		, m_io_keyboard(*this, "KEY")
	{ }

	DECLARE_DRIVER_INIT(excali64);
	DECLARE_PALETTE_INIT(excali64);
	DECLARE_WRITE8_MEMBER(ppib_w);
	DECLARE_READ8_MEMBER(ppic_r);
	DECLARE_WRITE8_MEMBER(ppic_w);
	DECLARE_READ8_MEMBER(port00_r);
	DECLARE_READ8_MEMBER(port50_r);
	DECLARE_WRITE8_MEMBER(port70_w);
	DECLARE_WRITE8_MEMBER(video_w);
	MC6845_UPDATE_ROW(update_row);
	DECLARE_WRITE_LINE_MEMBER(crtc_de);
	DECLARE_WRITE_LINE_MEMBER(crtc_vs);

	required_device<palette_device> m_palette;
	
private:
	const UINT8 *m_p_chargen;
	UINT8 *m_p_videoram;
	UINT8 m_sys_status;
	UINT8 m_kbdrow;
	bool m_crtc_vs;
	bool m_crtc_de;
	required_device<cpu_device> m_maincpu;
	required_device<cassette_image_device> m_cass;
	required_device<mc6845_device> m_crtc;
	required_ioport_array<8> m_io_keyboard;
};

static ADDRESS_MAP_START(excali64_mem, AS_PROGRAM, 8, excali64_state)
	AM_RANGE(0x0000, 0x1FFF) AM_ROM
	AM_RANGE(0x2000, 0x3FFF) AM_ROM AM_WRITE(video_w)
	AM_RANGE(0x4000, 0xFFFF) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START(excali64_io, AS_IO, 8, excali64_state)
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x0f) AM_READ(port00_r)
	AM_RANGE(0x10, 0x10) AM_MIRROR(0x0e) AM_DEVREADWRITE("uart",i8251_device, data_r, data_w)
	AM_RANGE(0x11, 0x11) AM_MIRROR(0x0e) AM_DEVREADWRITE("uart", i8251_device, status_r, control_w)
	AM_RANGE(0x20, 0x23) AM_MIRROR(0x0c) AM_DEVREADWRITE("pit", pit8253_device, read, write)
	AM_RANGE(0x30, 0x30) AM_MIRROR(0x0e) AM_DEVREADWRITE("crtc", mc6845_device, status_r, address_w)
	AM_RANGE(0x31, 0x31) AM_MIRROR(0x0e) AM_DEVREADWRITE("crtc", mc6845_device, register_r, register_w)
	AM_RANGE(0x50, 0x5f) AM_READ(port50_r)
	AM_RANGE(0x60, 0x63) AM_MIRROR(0x0c) AM_DEVREADWRITE("ppi", i8255_device, read, write)
	AM_RANGE(0x70, 0x7f) AM_WRITE(port70_w)
ADDRESS_MAP_END


/* Input ports */
static INPUT_PORTS_START( excali64 )
	PORT_START("KEY.0")    /* line 0 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R) PORT_CHAR('r') PORT_CHAR('R') PORT_CHAR(0x12)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W) PORT_CHAR('w') PORT_CHAR('W') PORT_CHAR(0x17)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_LSHIFT) PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E) PORT_CHAR('e') PORT_CHAR('E') PORT_CHAR(0x05)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("TAB") PORT_CODE(KEYCODE_TAB) PORT_CHAR(0x09)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("CAPSLOCK") PORT_CODE(KEYCODE_CAPSLOCK) PORT_TOGGLE
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A) PORT_CHAR('a') PORT_CHAR('A') PORT_CHAR(0x01)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q) PORT_CHAR('q') PORT_CHAR('Q') PORT_CHAR(0x11)

	PORT_START("KEY.1")    /* line 1 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F3") PORT_CODE(KEYCODE_F3)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F2") PORT_CODE(KEYCODE_F2)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F4") PORT_CODE(KEYCODE_F4)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("CTRL") PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(KEYCODE_RCONTROL) PORT_CHAR(UCHAR_SHIFT_2)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED) // space
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F1") PORT_CODE(KEYCODE_F1)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) // F1

	PORT_START("KEY.2")    /* line 2 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(". >") PORT_CODE(KEYCODE_STOP) PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M) PORT_CHAR('m') PORT_CHAR('M') PORT_CHAR(0x0d)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("/ ?") PORT_CODE(KEYCODE_SLASH) PORT_CHAR('/') PORT_CHAR('?')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(", <") PORT_CODE(KEYCODE_COMMA) PORT_CHAR(',') PORT_CHAR('<')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B) PORT_CHAR('b') PORT_CHAR('B') PORT_CHAR(0x02)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_UNUSED) //B
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N) PORT_CHAR('n') PORT_CHAR('N') PORT_CHAR(0x0e)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) //N

	PORT_START("KEY.3")    /* line 3 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("' \"") PORT_CODE(KEYCODE_QUOTE) PORT_CHAR(0x27) PORT_CHAR(0x22) PORT_CHAR(0x27)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L) PORT_CHAR('l') PORT_CHAR('L') PORT_CHAR(0x0c)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("RETURN") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(0x0d)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("; :") PORT_CODE(KEYCODE_COLON) PORT_CHAR(';') PORT_CHAR(':')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J) PORT_CHAR('j') PORT_CHAR('J') PORT_CHAR(0x0a)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G) PORT_CHAR('g') PORT_CHAR('G') PORT_CHAR(0x07)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K) PORT_CHAR('k') PORT_CHAR('K') PORT_CHAR(0x0b)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H) PORT_CHAR('h') PORT_CHAR('H') PORT_CHAR(0x08)

	PORT_START("KEY.4")    /* line 4 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4 $") PORT_CODE(KEYCODE_4) PORT_CHAR('4') PORT_CHAR('$')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2 @") PORT_CODE(KEYCODE_2) PORT_CHAR('2') PORT_CHAR('@')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5 %") PORT_CODE(KEYCODE_5) PORT_CHAR('5') PORT_CHAR('%')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3 #") PORT_CODE(KEYCODE_3) PORT_CHAR('3') PORT_CHAR('#')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("DEL") PORT_CODE(KEYCODE_DEL) PORT_CHAR(0x7f) PORT_CHAR(0x7f) PORT_CHAR(0x1f)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("ESC") PORT_CODE(KEYCODE_ESC) PORT_CHAR(0x1b)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1 !") PORT_CODE(KEYCODE_1) PORT_CHAR('1') PORT_CHAR('!')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) //1

	PORT_START("KEY.5")    /* line 5 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("[ {") PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR('[') PORT_CHAR('{') PORT_CHAR(0x1b)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O) PORT_CHAR('o') PORT_CHAR('O') PORT_CHAR(0x0f)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("] }") PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(']') PORT_CHAR('}') PORT_CHAR(0x1d)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P) PORT_CHAR('p') PORT_CHAR('P') PORT_CHAR(0x10)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U) PORT_CHAR('u') PORT_CHAR('U') PORT_CHAR(0x15)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T) PORT_CHAR('t') PORT_CHAR('T') PORT_CHAR(0x14)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I) PORT_CHAR('i') PORT_CHAR('I') PORT_CHAR(0x09)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Y) PORT_CHAR('y') PORT_CHAR('Y') PORT_CHAR(0x19)

	PORT_START("KEY.6")    /* line 6 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F) PORT_CHAR('f') PORT_CHAR('F') PORT_CHAR(0x06)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X) PORT_CHAR('x') PORT_CHAR('X') PORT_CHAR(0x18)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V) PORT_CHAR('v') PORT_CHAR('V') PORT_CHAR(0x16)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C) PORT_CHAR('c') PORT_CHAR('C') PORT_CHAR(0x03)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D) PORT_CHAR('d') PORT_CHAR('D') PORT_CHAR(0x04)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S) PORT_CHAR('s') PORT_CHAR('S') PORT_CHAR(0x13)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Z) PORT_CHAR('z') PORT_CHAR('Z') PORT_CHAR(0x1a)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_UNUSED) //Z

	PORT_START("KEY.7")    /* line 7 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("= +") PORT_CODE(KEYCODE_EQUALS) PORT_CHAR('=') PORT_CHAR('+') PORT_CHAR('=')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0 )") PORT_CODE(KEYCODE_0) PORT_CHAR('0') PORT_CHAR(')')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Backspace") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(0x08)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("- _") PORT_CODE(KEYCODE_MINUS) PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8 *") PORT_CODE(KEYCODE_8) PORT_CHAR('8') PORT_CHAR('*')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6 ^") PORT_CODE(KEYCODE_6) PORT_CHAR('6') PORT_CHAR('^')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9 (") PORT_CODE(KEYCODE_9) PORT_CHAR('9') PORT_CHAR('(')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7 &") PORT_CODE(KEYCODE_7) PORT_CHAR('7') PORT_CHAR('&')
INPUT_PORTS_END

WRITE8_MEMBER( excali64_state::ppib_w )
{
	m_kbdrow = data;
}

READ8_MEMBER( excali64_state::ppic_r )
{
	UINT8 data = 0xf7;
	data |= (m_cass->input() > 0.1) << 3;
	return data;
}

READ8_MEMBER( excali64_state::port00_r )
{
	UINT8 data = 0xff;

	for (int i = 0; i < 8; i++)
	{
		if (!BIT(m_kbdrow, i))
			data &= m_io_keyboard[i]->read();
	}

	return data;
}

/*
d0 : /rom ; screen
d1 : ram on
d2 : /low ; high res
d3 : dispen
d4 : vsync
*/
READ8_MEMBER( excali64_state::port50_r )
{
	UINT8 data = m_sys_status & 7;
	data |= (UINT8)m_crtc_vs << 4;
	data |= (UINT8)m_crtc_de << 5;
	return data;
}

WRITE8_MEMBER( excali64_state::ppic_w )
{
	m_cass->output(BIT(data, 7) ? -1.0 : +1.0);
}

/*
d0,1,2 : same as port50
d3 : 2nd colour set
*/
WRITE8_MEMBER( excali64_state::port70_w )
{
	m_sys_status = data;
	m_crtc->set_unscaled_clock(BIT(data, 2) ? 2e6 : 1e6);
}

WRITE8_MEMBER( excali64_state::video_w )
{
	m_p_videoram[offset] = data;
}

WRITE_LINE_MEMBER( excali64_state::crtc_de )
{
	m_crtc_de = state;
}

WRITE_LINE_MEMBER( excali64_state::crtc_vs )
{
	m_crtc_vs = state;
}

DRIVER_INIT_MEMBER( excali64_state, excali64 )
{
	m_p_chargen = memregion("chargen")->base();
	m_p_videoram = memregion("videoram")->base();
}

/* F4 Character Displayer */
static const gfx_layout excali64_charlayout =
{
	8, 12,                  /* 8 x 12 characters */
	256,                    /* 256 characters */
	1,                      /* 1 bits per pixel */
	{ 0 },                  /* no bitplanes */
	/* x offsets */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8 },
	8*16                    /* every char takes 16 bytes */
};

static GFXDECODE_START( excali64 )
	GFXDECODE_ENTRY( "chargen", 0x0000, excali64_charlayout, 0, 1 )
GFXDECODE_END

// The colour names in the comments are what's needed, the current rgb values are mostly wrong
PALETTE_INIT_MEMBER( excali64_state, excali64 )
{
	// Colour Menu A
	palette.set_pen_color(0, 0x00, 0x00, 0x00);   /*  0 Black     */
	palette.set_pen_color(1, 0x7f, 0x00, 0x00);   /*  1 Dark Red      */
	palette.set_pen_color(2, 0xff, 0x00, 0x00);   /*  2 Red       */
	palette.set_pen_color(3, 0x00, 0x00, 0x00);   /*  3 Pink     */
	palette.set_pen_color(4, 0xbf, 0xbf, 0xbf);   /*  4 Orange     */
	palette.set_pen_color(5, 0x00, 0xff, 0xff);   /*  5 Brown     */
	palette.set_pen_color(6, 0xff, 0xff, 0x00);   /*  6 Yellow        */
	palette.set_pen_color(7, 0x7f, 0x7f, 0x00);   /*  7 Dark Green */
	palette.set_pen_color(8, 0x00, 0x7f, 0x00);   /*  8 Green     */
	palette.set_pen_color(9, 0x00, 0xff, 0x00);   /*  9 Bright Green  */
	palette.set_pen_color(10, 0x00, 0x00, 0xff);  /* 10 Light Blue    */
	palette.set_pen_color(11, 0x00, 0x00, 0x7f);  /* 11 Blue      */
	palette.set_pen_color(12, 0xff, 0x00, 0xff);  /* 12 Magenta       */
	palette.set_pen_color(13, 0x7f, 0x00, 0x7f);  /* 13 Purple        */
	palette.set_pen_color(14, 0x80, 0x80, 0x80);  /* 14 Dark Grey      */
	palette.set_pen_color(15, 0xff, 0xff, 0xff);  /* 15 White     */
	// Colour Menu B
	palette.set_pen_color(16, 0x00, 0x00, 0x00);  /*  0 Black     */
	palette.set_pen_color(17, 0x7f, 0x00, 0x00);  /*  1 Dark Red  */
	palette.set_pen_color(18, 0xff, 0x00, 0x00);  /*  2 Red       */
	palette.set_pen_color(19, 0x80, 0x80, 0x80);  /*  3 Flesh     */
	palette.set_pen_color(20, 0x00, 0x00, 0xff);  /*  4 Pink      */
	palette.set_pen_color(21, 0xff, 0xff, 0x80);  /*  5 Yellow Brown */
	palette.set_pen_color(22, 0x00, 0x00, 0x00);  /*  6 Dark Brown     */
	palette.set_pen_color(23, 0x00, 0xff, 0x00);  /*  7 Dark Purple */
	palette.set_pen_color(24, 0xff, 0x80, 0xff);  /*  8 Very Dark Green */
	palette.set_pen_color(25, 0x00, 0xff, 0xff);  /*  9 Yellow Green */
	palette.set_pen_color(26, 0xff, 0x40, 0x40);  /* 10 Grey Blue */
	palette.set_pen_color(27, 0xff, 0x00, 0x00);  /* 11 Sky Blue */
	palette.set_pen_color(28, 0x00, 0x80, 0x80);  /* 12 Very Pale Blue */
	palette.set_pen_color(29, 0xff, 0x00, 0xff);  /* 13 Dark Grey */
	palette.set_pen_color(30, 0x80, 0xff, 0x80);  /* 14 Light Grey */
	palette.set_pen_color(31, 0xff, 0xff, 0xff);  /* 15 White     */
	// Background
	palette.set_pen_color(32, 0x00, 0x00, 0x00);  //  0 Black
	palette.set_pen_color(33, 0xff, 0x00, 0x00);  //  1 Red
	palette.set_pen_color(34, 0x00, 0x00, 0xff);  //  2 Blue
	palette.set_pen_color(35, 0xff, 0x00, 0xff);  //  3 Magenta
	palette.set_pen_color(36, 0x00, 0xff, 0x00);  //  4 Green
	palette.set_pen_color(37, 0xff, 0xff, 0x00);  //  5 Yellow
	palette.set_pen_color(38, 0x00, 0xff, 0xff);  //  6 Cyan
	palette.set_pen_color(39, 0xff, 0xff, 0xff);  //  7 White

}

MC6845_UPDATE_ROW( excali64_state::update_row )
{
	const rgb_t *palette = m_palette->palette()->entry_list_raw();
	UINT8 chr,gfx,col,bg,fg;
	UINT16 mem,x;
	UINT8 col_base = BIT(m_sys_status, 3) ? 16 : 0;
	UINT32 *p = &bitmap.pix32(y);

	for (x = 0; x < x_count; x++)
	{
		mem = (ma + x) & 0x7ff;
		chr = m_p_videoram[mem];
		col = m_p_videoram[mem+0x800];
		fg = col_base + (col >> 4);
		bg = 32 + ((col >> 1) & 7);

		if (BIT(col, 0) & BIT(chr, 7))
			gfx = m_p_videoram[0x800 + (chr<<4) + ra]; // hires definition
		else
			gfx = m_p_chargen[(chr<<4) | ra]; // normal character
		
		gfx ^= ((x == cursor_x) ? 0xff : 0);

		/* Display a scanline of a character */
		*p++ = palette[BIT(gfx, 0) ? fg : bg];
		*p++ = palette[BIT(gfx, 1) ? fg : bg];
		*p++ = palette[BIT(gfx, 2) ? fg : bg];
		*p++ = palette[BIT(gfx, 3) ? fg : bg];
		*p++ = palette[BIT(gfx, 4) ? fg : bg];
		*p++ = palette[BIT(gfx, 5) ? fg : bg];
		*p++ = palette[BIT(gfx, 6) ? fg : bg];
		*p++ = palette[BIT(gfx, 7) ? fg : bg];
	}
}

static MACHINE_CONFIG_START( excali64, excali64_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z80, XTAL_16MHz / 4)
	MCFG_CPU_PROGRAM_MAP(excali64_mem)
	MCFG_CPU_IO_MAP(excali64_io)

	MCFG_DEVICE_ADD("uart", I8251, 0)
	//MCFG_I8251_TXD_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_txd))
	//MCFG_I8251_RTS_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_rts))

	MCFG_DEVICE_ADD("pit", PIT8253, 0)
	MCFG_PIT8253_CLK0(XTAL_16MHz / 16) /* Timer 0: tone gen for speaker */
	MCFG_PIT8253_OUT0_HANDLER(DEVWRITELINE("speaker", speaker_sound_device, level_w))
	MCFG_PIT8253_CLK1(XTAL_16MHz / 8) /* Timer 1: baud rate gen for 8251 */
	//MCFG_PIT8253_OUT1_HANDLER(WRITELINE(excali64_state, write_uart_clock))
	//MCFG_PIT8253_CLK2(XTAL_16MHz / 8) /* Timer 2: not used */

	MCFG_DEVICE_ADD("ppi", I8255A, 0 )
	//MCFG_I8255_IN_PORTA_CB(READ8(excali64_state, ppia_r))
	//MCFG_I8255_OUT_PORTA_CB(WRITE8(excali64_state, ppia_w)) // parallel port
	//MCFG_I8255_IN_PORTB_CB(READ8(excali64_state, ppib_r))
	MCFG_I8255_OUT_PORTB_CB(WRITE8(excali64_state, ppib_w))
	MCFG_I8255_IN_PORTC_CB(READ8(excali64_state, ppic_r))
	MCFG_I8255_OUT_PORTC_CB(WRITE8(excali64_state, ppic_w))

	//MCFG_DEVICE_ADD("acia_clock", CLOCK, 153600)
	//MCFG_CLOCK_SIGNAL_HANDLER(WRITELINE(excali64_state, write_acia_clock))

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
	MCFG_SOUND_WAVE_ADD(WAVE_TAG, "cassette")
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)

	/* Video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_SIZE(80*8, 24*12)
	MCFG_SCREEN_VISIBLE_AREA(0, 80*8-1, 0, 24*12-1)
	MCFG_SCREEN_UPDATE_DEVICE("crtc", mc6845_device, screen_update)
	MCFG_PALETTE_ADD("palette", 40)
	MCFG_PALETTE_INIT_OWNER(excali64_state, excali64)
	//MCFG_PALETTE_ADD_BLACK_AND_WHITE("palette")
	MCFG_GFXDECODE_ADD("gfxdecode", "palette", excali64)
	MCFG_MC6845_ADD("crtc", MC6845, "screen", XTAL_16MHz / 16) // 1MHz for lowres; 2MHz for highres
	MCFG_MC6845_SHOW_BORDER_AREA(false)
	MCFG_MC6845_CHAR_WIDTH(8)
	MCFG_MC6845_UPDATE_ROW_CB(excali64_state, update_row)
	MCFG_MC6845_OUT_DE_CB(WRITELINE(excali64_state, crtc_de))
	MCFG_MC6845_OUT_VSYNC_CB(WRITELINE(excali64_state, crtc_vs))

	/* Devices */
	MCFG_CASSETTE_ADD( "cassette" )
	MACHINE_CONFIG_END

/* ROM definition */
ROM_START( excali64 )
	ROM_REGION(0x10000, "maincpu", 0)
	ROM_LOAD( "rom_1.ic17", 0x0000, 0x4000, CRC(e129a305) SHA1(e43ec7d040c2b2e548d22fd6bbc7df8b45a26e5a) )
	ROM_LOAD( "rom_2.ic24", 0x2000, 0x2000, CRC(916d9f5a) SHA1(91c527cce963481b7bebf077e955ca89578bb553) )
	// fix a bug that causes screen to be filled with 'p'
	ROM_FILL(0x4ee, 1, 0)
	ROM_FILL(0x4ef, 1, 8)
	ROM_FILL(0x4f6, 1, 0)
	ROM_FILL(0x4f7, 1, 8)

	ROM_REGION(0x2000, "videoram", ROMREGION_ERASE00)

	ROM_REGION(0x1000, "chargen", 0)
	ROM_LOAD( "genex_3.ic43", 0x0000, 0x1000, CRC(b91619a9) SHA1(2ced636cb7b94ba9d329868d7ecf79963cefe9d9) )
ROM_END

/* Driver */

/*    YEAR  NAME      PARENT  COMPAT   MACHINE    INPUT     CLASS             INIT        COMPANY         FULLNAME        FLAGS */
COMP( 1984, excali64, 0,      0,       excali64,  excali64, excali64_state, excali64,  "BGR Computers", "Excalibur 64", GAME_NOT_WORKING )
