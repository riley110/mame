// license:BSD-3-Clause
// copyright-holders:hap, Sean Riddle
/***************************************************************************

  Sharp SM510 MCU x..

  TODO:
  - barnacles

***************************************************************************/

#include "emu.h"
#include "cpu/sm510/sm510.h"
#include "sound/speaker.h"

// internal artwork
//..


class hh_sm510_state : public driver_device
{
public:
	hh_sm510_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_inp_matrix(*this, "IN"),
		m_speaker(*this, "speaker")
	{ }

	// devices
	required_device<cpu_device> m_maincpu;
	optional_ioport_array<5> m_inp_matrix; // max 5
	optional_device<speaker_sound_device> m_speaker;

	// misc common
	UINT16 m_inp_mux;                   // multiplexed inputs mask

	UINT8 read_inputs(int columns);

	// display common
	//..

protected:
	virtual void machine_start();
	virtual void machine_reset();
};


// machine start/reset

void hh_sm510_state::machine_start()
{
	// zerofill
	m_inp_mux = 0;

	// register for savestates
	save_item(NAME(m_inp_mux));
}

void hh_sm510_state::machine_reset()
{
}



/***************************************************************************

  Helper Functions

***************************************************************************/

UINT8 hh_sm510_state::read_inputs(int columns)
{
	UINT8 ret = 0;

	// read selected input rows
	for (int i = 0; i < columns; i++)
		if (m_inp_mux >> i & 1)
			ret |= m_inp_matrix[i]->read();

	return ret;
}



/***************************************************************************

  Minidrivers (subclass, I/O, Inputs, Machine Config)

***************************************************************************/

/***************************************************************************

  Konami Top Gun
  * x

***************************************************************************/

class ktopgun_state : public hh_sm510_state
{
public:
	ktopgun_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE8_MEMBER(input_w);
	DECLARE_READ8_MEMBER(input_r);
};

// handlers


WRITE8_MEMBER(ktopgun_state::input_w)
{
	// S1-S3: input mux
	m_inp_mux = data;
}

READ8_MEMBER(ktopgun_state::input_r)
{
	//printf("%02X ",m_inp_mux);
	return read_inputs(3);
}



// config

static INPUT_PORTS_START( ktopgun )
	PORT_START("IN.0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 )

	PORT_START("IN.1")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )

	PORT_START("IN.2")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON5 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON6 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON7 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON8 )
INPUT_PORTS_END

static MACHINE_CONFIG_START( ktopgun, ktopgun_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, XTAL_32_768kHz)
	MCFG_SM510_READ_K_CB(READ8(ktopgun_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(ktopgun_state, input_w))

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END








/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( ktopgun )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "topgun_die.bin", 0x0000, 0x1000, CRC(50870b35) SHA1(cda1260c2e1c180995eced04b7d7ff51616dcef5) )
ROM_END






/*    YEAR  NAME       PARENT COMPAT MACHINE   INPUT      INIT              COMPANY, FULLNAME, FLAGS */
CONS( 1989, ktopgun,   0,        0, ktopgun,   ktopgun,   driver_device, 0, "Konami", "Top Gun (Konami)", GAME_SUPPORTS_SAVE | GAME_NOT_WORKING )
