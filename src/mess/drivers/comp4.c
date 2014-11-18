// license:BSD-3-Clause
// copyright-holders:hap
/***************************************************************************

  Milton Bradley Comp IV
  * TMC0904NL CP0904A (die labeled 4A0970D-04A)
  
  This is a handheld Mastermind game; a code-breaking game where the player
  needs to find out the correct sequence of colours (numbers in our case).
  It is known as Logic 5 in Europe, and as Pythaligoras in Japan.
  
  Press the R key to start, followed by a set of unique numbers and E.
  Refer to the official manual for more information.


  TODO:
  - write_r doesn't look right, maybe something missing in cpu emulation
  - layout

***************************************************************************/

#include "emu.h"
#include "cpu/tms0980/tms0980.h"

// master clock is cpu internal, the value below is an approximation
#define MASTER_CLOCK (250000)


class comp4_state : public driver_device
{
public:
	comp4_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_button_matrix(*this, "IN")
	{ }

	required_device<cpu_device> m_maincpu;
	required_ioport_array<3> m_button_matrix;

	UINT16 m_r;
	UINT16 m_o;

	DECLARE_READ8_MEMBER(read_k);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_WRITE16_MEMBER(write_r);

	virtual void machine_start();
};


/***************************************************************************

  I/O

***************************************************************************/

READ8_MEMBER(comp4_state::read_k)
{
	UINT8 k = 0;

	// read selected button rows
	for (int i = 0; i < 3; i++)
		if (m_o & (1 << (i + 1)))
			k |= m_button_matrix[i]->read();
	
	return k;
}

WRITE16_MEMBER(comp4_state::write_r)
{
	// R..: LEDs
	m_r = data;
}

WRITE16_MEMBER(comp4_state::write_o)
{
	// O0?: LEDs (common)
	// O1-O3: input mux
	// other bits: N/C
	m_o = data;
}



/***************************************************************************

  Inputs

***************************************************************************/

static INPUT_PORTS_START( comp4 )
	PORT_START("IN.0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_R) PORT_CODE(KEYCODE_DEL_PAD) PORT_NAME("R")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")

	PORT_START("IN.1")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("0")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")

	PORT_START("IN.2")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_E) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("E")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9")
INPUT_PORTS_END



/***************************************************************************

  Machine Config

***************************************************************************/

void comp4_state::machine_start()
{
	m_r = 0;
	m_o = 0;

	save_item(NAME(m_r));
	save_item(NAME(m_o));
}


static const UINT16 comp4_output_pla[0x20] =
{
	// many unused bits, only O0 is actually used as external out
	0xda, 0x96, 0x9b, 0x97, 0x98, 0x94, 0x99, 0x95,
	0x9a, 0xa0, 0x94, 0x02, 0x35, 0x4a, 0x41, 0x00,
	0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00,
	0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00
};


static MACHINE_CONFIG_START( comp4, comp4_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS0970, MASTER_CLOCK)
	MCFG_TMS1XXX_OUTPUT_PLA(comp4_output_pla)
	MCFG_TMS1XXX_READ_K(READ8(comp4_state, read_k))
	MCFG_TMS1XXX_WRITE_O(WRITE16(comp4_state, write_o))
	MCFG_TMS1XXX_WRITE_R(WRITE16(comp4_state, write_r))

	/* no video! */

	/* no sound! */
MACHINE_CONFIG_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( comp4 )
	ROM_REGION( 0x0400, "maincpu", 0 )
	ROM_LOAD( "cp0904a", 0x0000, 0x0400, CRC(c502c8a1) SHA1(f82ff1a85c4849621d32344964d8b2233fc978d0) )
ROM_END


CONS( 1977, comp4, 0, 0, comp4, comp4, driver_device, 0, "Milton Bradley", "Comp IV", GAME_NOT_WORKING | GAME_SUPPORTS_SAVE | GAME_NO_SOUND_HW )
