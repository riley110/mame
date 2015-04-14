// license:BSD-3-Clause
// copyright-holders:hap, Sean Riddle, Kevin Horton
/***************************************************************************

  This driver is a collection of simple dedicated handheld and tabletop
  toys based around the TMS1000 MCU series. Anything more complex or clearly
  part of a series is (or will be) in its own driver.

  Let's use this driver for a list of known devices and their serials,
  excluding TI's own products (see ticalc1x.c, tispeak.c)

  serial   device   etc.
--------------------------------------------------------------------
 @CP0904A  TMS0970  1977, Milton Bradley Comp IV
 @MP0905B  TMS0970  1977, Parker Brothers Codename Sector
 *MP0168   TMS1000? 1979, Conic Basketball
 @MP0914   TMS1000  1979, Entex Baseball 1
 @MP0923   TMS1000  1979, Entex Baseball 2
 @MP1030   TMS1100  1980, APF Mathemagician
 @MP1133   TMS1470  1979, Kosmos Astro
 @MP1204   TMS1100  1980, Entex Baseball 3
 @MP1211   TMS1100  1980, Entex Space Invader
 *MP1221   TMS1100  1980, Entex Raise The Devil
 *MP1312   TMS1100  198?, Tandy/RadioShack Science Fair Microcomputer Trainer
 *MP2105   TMS1370  1979, Gakken Poker, Entex Electronic Poker
 *MP2139   TMS1370? 1982, Gakken Galaxy Invader 1000
 *MP2788   ?        1980, Bandai Flight Time (? note: VFD-capable)
 @MP3226   TMS1000  1978, Milton Bradley Simon
 *MP3301   TMS1000  1979, Milton Bradley Big Trak
 *MP3320A  TMS1000  1979, Coleco Head to Head Basketball
  MP3403   TMS1100  1978, Marx Electronic Bowling -> elecbowl.c
 @MP3404   TMS1100  1978, Parker Brothers Merlin
 @MP3405   TMS1100  1979, Coleco Amaze-A-Tron
 @MP3438A  TMS1100  1979, Kenner Star Wars Electronic Battle Command
  MP3450A  TMS1100  1979, MicroVision cartridge: Blockbuster
  MP3454   TMS1100  1979, MicroVision cartridge: Star Trek Phaser Strike
  MP3455   TMS1100  1980, MicroVision cartridge: Pinball
  MP3457   TMS1100  1979, MicroVision cartridge: Mindbuster
  MP3474   TMS1100  1979, MicroVision cartridge: Vegas Slots
  MP3475   TMS1100  1979, MicroVision cartridge: Bowling
 @MP3476   TMS1100  1979, Milton Bradley Super Simon
  MP3479   TMS1100  1980, MicroVision cartridge: Baseball
  MP3481   TMS1100  1979, MicroVision cartridge: Connect Four
  MP3496   TMS1100  1980, MicroVision cartridge: Sea Duel
  M34009   TMS1100  1981, MicroVision cartridge: Alien Raiders (note: MP3498, MP3499, M34000, ..)
  M34017   TMS1100  1981, MicroVision cartridge: Cosmic Hunter
  M34047   TMS1100  1982, MicroVision cartridge: Super Blockbuster
 @MP6100A  TMS0980  1979, Ideal Electronic Detective
 @MP6101B  TMS0980  1979, Parker Brothers Stop Thief
 *MP6361   ?        1983, Defender Strikes (? note: VFD-capable)
 *MP7303   TMS1400? 19??, Tiger 7-in-1 Sports Stadium
 @MP7313   TMS1400  1980, Parker Brothers Bank Shot
 @MP7314   TMS1400  1980, Parker Brothers Split Second
 *MP7324   TMS1400? 1985, Coleco Talking Teacher
  MP7332   TMS1400  1981, Milton Bradley Dark Tower -> mbdtower.c
 @MP7334   TMS1400  1981, Coleco Total Control 4
 *MP7573   ?        1981, Entex Select-a-Game cartridge: Football (? note: 40-pin, VFD-capable)

  inconsistent:

 *MPF553   TMS1670  1980, Entex Jackpot Gin Rummy Black Jack
 *M95041   ?        1983, Tsukuda Game Pachinko (? note: 40-pin, VFD-capable)
 @CD7282SL TMS1100  1981, Tandy/RadioShack Tandy-12 (serial is similar to TI Speak & Spell series?)

  (* denotes not yet emulated by MESS, @ denotes it's in this driver)


  TODO:
  - verify output PLA and microinstructions PLA for MCUs that have been dumped
    electronically (mpla is usually the default, opla is often custom)
  - unknown MCU clocks for some: TMS1000 and TMS1100 RC curve is documented in
    the data manual, but for TMS1400 it's unknown. TMS0970/0980 osc. is on-die.
  - some of the games rely on the fact that faster/longer strobed leds appear
    brighter: tc4(offensive players), bankshot(cue ball), ...
  - add softwarelist for tc4 cartridges?
  - stopthiep: unable to start a game (may be intentional?)

***************************************************************************/

#include "includes/hh_tms1k.h"

// internal artwork
#include "amaztron.lh"
#include "astro.lh"
#include "bankshot.lh"
#include "cnsector.lh"
#include "ebball.lh"
#include "ebball2.lh"
#include "ebball3.lh"
#include "einvader.lh" // test-layout(but still playable)
#include "elecdet.lh"
#include "comp4.lh"
#include "mathmagi.lh"
#include "merlin.lh" // clickable
#include "simon.lh" // clickable
#include "ssimon.lh" // clickable
#include "splitsec.lh"
#include "starwbc.lh"
#include "stopthie.lh"
#include "tandy12.lh" // clickable
#include "tc4.lh"


// machine_start/reset

void hh_tms1k_state::machine_start()
{
	// zerofill
	memset(m_display_state, 0, sizeof(m_display_state));
	memset(m_display_cache, ~0, sizeof(m_display_cache));
	memset(m_display_decay, 0, sizeof(m_display_decay));
	memset(m_display_segmask, 0, sizeof(m_display_segmask));

	m_o = 0;
	m_r = 0;
	m_inp_mux = 0;
	m_power_on = false;

	// register for savestates
	save_item(NAME(m_display_maxy));
	save_item(NAME(m_display_maxx));
	save_item(NAME(m_display_wait));

	save_item(NAME(m_display_state));
	/* save_item(NAME(m_display_cache)); */ // don't save!
	save_item(NAME(m_display_decay));
	save_item(NAME(m_display_segmask));

	save_item(NAME(m_o));
	save_item(NAME(m_r));
	save_item(NAME(m_inp_mux));
	save_item(NAME(m_power_on));
}

void hh_tms1k_state::machine_reset()
{
	m_power_on = true;
}



/***************************************************************************

  Helper Functions

***************************************************************************/

// The device may strobe the outputs very fast, it is unnoticeable to the user.
// To prevent flickering here, we need to simulate a decay.

void hh_tms1k_state::display_update()
{
	UINT32 active_state[0x20];

	for (int y = 0; y < m_display_maxy; y++)
	{
		active_state[y] = 0;

		for (int x = 0; x <= m_display_maxx; x++)
		{
			// turn on powered segments
			if (m_power_on && m_display_state[y] >> x & 1)
				m_display_decay[y][x] = m_display_wait;

			// determine active state
			UINT32 ds = (m_display_decay[y][x] != 0) ? 1 : 0;
			active_state[y] |= (ds << x);
		}
	}

	// on difference, send to output
	for (int y = 0; y < m_display_maxy; y++)
		if (m_display_cache[y] != active_state[y])
		{
			if (m_display_segmask[y] != 0)
				output_set_digit_value(y, active_state[y] & m_display_segmask[y]);

			const int mul = (m_display_maxx <= 10) ? 10 : 100;
			for (int x = 0; x <= m_display_maxx; x++)
			{
				int state = active_state[y] >> x & 1;
				char buf1[0x10]; // lampyx
				char buf2[0x10]; // y.x

				if (x == m_display_maxx)
				{
					// always-on if selected
					sprintf(buf1, "lamp%da", y);
					sprintf(buf2, "%d.a", y);
				}
				else
				{
					sprintf(buf1, "lamp%d", y * mul + x);
					sprintf(buf2, "%d.%d", y, x);
				}
				output_set_value(buf1, state);
				output_set_value(buf2, state);
			}
		}

	memcpy(m_display_cache, active_state, sizeof(m_display_cache));
}

TIMER_DEVICE_CALLBACK_MEMBER(hh_tms1k_state::display_decay_tick)
{
	// slowly turn off unpowered segments
	for (int y = 0; y < m_display_maxy; y++)
		for (int x = 0; x <= m_display_maxx; x++)
			if (m_display_decay[y][x] != 0)
				m_display_decay[y][x]--;

	display_update();
}

void hh_tms1k_state::set_display_size(int maxx, int maxy)
{
	m_display_maxx = maxx;
	m_display_maxy = maxy;
}

void hh_tms1k_state::display_matrix(int maxx, int maxy, UINT32 setx, UINT32 sety)
{
	set_display_size(maxx, maxy);

	// update current state
	UINT32 mask = (1 << maxx) - 1;
	for (int y = 0; y < maxy; y++)
		m_display_state[y] = (sety >> y & 1) ? ((setx & mask) | (1 << maxx)) : 0;

	display_update();
}

void hh_tms1k_state::display_matrix_seg(int maxx, int maxy, UINT32 setx, UINT32 sety, UINT16 segmask)
{
	// expects m_display_segmask to be not-0
	for (int y = 0; y < maxy; y++)
		m_display_segmask[y] &= segmask;

	display_matrix(maxx, maxy, setx, sety);
}


UINT8 hh_tms1k_state::read_inputs(int columns)
{
	UINT8 ret = 0;

	// read selected input rows
	for (int i = 0; i < columns; i++)
		if (m_inp_mux >> i & 1)
			ret |= m_inp_matrix[i]->read();

	return ret;
}


// devices with a TMS0980 can auto power-off

WRITE_LINE_MEMBER(hh_tms1k_state::auto_power_off)
{
	if (state)
	{
		m_power_on = false;
		m_maincpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	}
}

INPUT_CHANGED_MEMBER(hh_tms1k_state::power_button)
{
	m_power_on = (bool)(FPTR)param;
	m_maincpu->set_input_line(INPUT_LINE_RESET, m_power_on ? CLEAR_LINE : ASSERT_LINE);
}



/***************************************************************************

  Minidrivers (subclass, I/O, Inputs, Machine Config)

***************************************************************************/

/***************************************************************************

  APF Mathemagician
  * TMS1100 MCU, labeled MP1030
  * 2 x DS8870N - Hex LED Digit Driver
  * 2 x DS8861N - MOS-to-LED 5-Segment Driver
  * 10-digit 7seg LED display(2 custom ones) + 4 LEDs, no sound

  This is a tabletop educational calculator. It came with plastic overlays
  for playing different kind of games. Refer to the manual on how to use it.
  In short, to start from scratch, press [SEL]. By default the device is in
  calculator teaching mode. If [SEL] is followed with 1-6 and then [NXT],
  one of the games is started.

  1) Number Machine
  2) Countin' On
  3) Walk The Plank
  4) Gooey Gumdrop
  5) Football
  6) Lunar Lander

***************************************************************************/

class mathmagi_state : public hh_tms1k_state
{
public:
	mathmagi_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void mathmagi_state::prepare_display()
{
	// R0-R7: 7seg leds
	for (int y = 0; y < 8; y++)
	{
		m_display_segmask[y] = 0x7f;
		m_display_state[y] = (m_r >> y & 1) ? (m_o >> 1) : 0;
	}

	// R8: custom math symbols digit
	// R9: custom equals digit
	// R10: misc lamps
	for (int y = 8; y < 11; y++)
		m_display_state[y] = (m_r >> y & 1) ? m_o : 0;

	set_display_size(8, 11);
	display_update();
}

WRITE16_MEMBER(mathmagi_state::write_r)
{
	// R3,R5-R7,R9,R10: input mux
	m_inp_mux = (data >> 3 & 1) | (data >> 4 & 0xe) | (data >> 5 & 0x30);

	// +others:
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(mathmagi_state::write_o)
{
	// O1-O7: led segments A-G
	// O0: N/C
	data = (data << 1 & 0xfe) | (data >> 7 & 1); // because opla is unknown
	m_o = data;
}

READ8_MEMBER(mathmagi_state::read_k)
{
	return read_inputs(6);
}


// config

/* physical button layout and labels is like this:

    ON     ONE       [SEL] [NXT] [?]   [/]
     |      |        [7]   [8]   [9]   [x]
    OFF    TWO       [4]   [5]   [6]   [-]
         PLAYERS     [1]   [2]   [3]   [+]
                     [0]   [_]   [r]   [=]
*/

static INPUT_PORTS_START( mathmagi )
	PORT_START("IN.0") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_MINUS_PAD) PORT_NAME("-")

	PORT_START("IN.1") // R5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("0")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_SPACE) PORT_NAME("_") // blank
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_R) PORT_NAME("r")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_PLUS_PAD) PORT_NAME("+")

	PORT_START("IN.2") // R6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_ASTERISK) PORT_NAME(UTF8_MULTIPLY)

	PORT_START("IN.3") // R7
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_S) PORT_NAME("SEL")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_N) PORT_NAME("NXT")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_C) PORT_NAME("?") // check
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("=")

	PORT_START("IN.4") // R9
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_SLASH_PAD) PORT_NAME(UTF8_DIVIDE)

	PORT_START("IN.5") // R10
	PORT_CONFNAME( 0x01, 0x00, "Players")
	PORT_CONFSETTING(    0x00, "1" )
	PORT_CONFSETTING(    0x01, "2" )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END


// output PLA is not dumped
static const UINT16 mathmagi_output_pla[0x20] =
{
	lA+lB+lC+lD+lE+lF,      // 0
	lB+lC,                  // 1
	lA+lB+lG+lE+lD,         // 2
	lA+lB+lG+lC+lD,         // 3
	lF+lB+lG+lC,            // 4
	lA+lF+lG+lC+lD,         // 5
	lA+lF+lG+lC+lD+lE,      // 6
	lA+lB+lC,               // 7
	lA+lB+lC+lD+lE+lF+lG,   // 8
	lA+lB+lG+lF+lC+lD,      // 9
	lA+lB+lG+lE,            // question mark
	lE+lG,                  // r
	lD,                     // underscore?
	lA+lF+lG+lE+lD,         // E
	lG,                     // -
	0,                      // empty
	0,                      // empty
	lG,                     // lamp 4 or MATH -
	lD,                     // lamp 3
	lF+lE+lD+lC+lG,         // b
	lB,                     // lamp 2
	lB+lG,                  // MATH +
	lB+lC,                  // MATH mul
	lF+lG+lB+lC+lD,         // y
	lA,                     // lamp 1
	lA+lG,                  // MATH div
	lA+lD,                  // EQUALS
	0,                      // ?
	0,                      // ?
	lE+lD+lC+lG,            // o
	0,                      // ?
	lA+lF+lE+lD+lC          // G
};

static MACHINE_CONFIG_START( mathmagi, mathmagi_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 175000) // RC osc. R=68K, C=82pf -> ~175kHz
	MCFG_TMS1XXX_OUTPUT_PLA(mathmagi_output_pla)
	MCFG_TMS1XXX_READ_K_CB(READ8(mathmagi_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(mathmagi_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(mathmagi_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_mathmagi)

	/* no video! */

	/* no sound! */
MACHINE_CONFIG_END





/***************************************************************************

  Coleco Amaze-A-Tron, by Ralph Baer
  * TMS1100 MCU, labeled MP3405(die label too)
  * 2-digit 7seg LED display + 2 LEDs(one red, one green), 1bit sound
  * 5x5 pressure-sensitive playing board

  This is an electronic board game with a selection of 8 maze games,
  most of them for 2 players. A 5x5 playing grid and four markers are
  required to play. Refer to the official manual for more information.

***************************************************************************/

class amaztron_state : public hh_tms1k_state
{
public:
	amaztron_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void amaztron_state::prepare_display()
{
	// R8,R9: select digit
	for (int y = 0; y < 2; y++)
	{
		m_display_segmask[y] = 0x7f;
		m_display_state[y] = (m_r >> (y + 8) & 1) ? m_o : 0;
	}

	// R6,R7: lamps (-> lamp20,21)
	m_display_state[2] = m_r >> 6 & 3;

	set_display_size(8, 3);
	display_update();
}

WRITE16_MEMBER(amaztron_state::write_r)
{
	// R0-R5: input mux
	m_inp_mux = data & 0x3f;

	// R10: speaker out
	m_speaker->level_w(data >> 10 & 1);

	// other bits:
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(amaztron_state::write_o)
{
	// O0-O6: led segments A-G
	// O7: N/C
	m_o = data & 0x7f;
	prepare_display();
}

READ8_MEMBER(amaztron_state::read_k)
{
	UINT8 k = read_inputs(6);

	// the 5th column is tied to K4+K8
	if (k & 0x10) k |= 0xc;
	return k & 0xf;
}


// config

static INPUT_PORTS_START( amaztron )
	PORT_START("IN.0") // R0
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_1) PORT_NAME("Button 1")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_6) PORT_NAME("Button 6")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_Q) PORT_NAME("Button 11")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_A) PORT_NAME("Button 16")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_Z) PORT_NAME("Button 21")

	PORT_START("IN.1") // R1
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_2) PORT_NAME("Button 2")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_7) PORT_NAME("Button 7")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_W) PORT_NAME("Button 12")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_S) PORT_NAME("Button 17")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_X) PORT_NAME("Button 22")

	PORT_START("IN.2") // R2
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_3) PORT_NAME("Button 3")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_8) PORT_NAME("Button 8")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_E) PORT_NAME("Button 13")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_D) PORT_NAME("Button 18")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_C) PORT_NAME("Button 23")

	PORT_START("IN.3") // R3
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_4) PORT_NAME("Button 4")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_9) PORT_NAME("Button 9")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_R) PORT_NAME("Button 14")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F) PORT_NAME("Button 19")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_V) PORT_NAME("Button 24")

	PORT_START("IN.4") // R4
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_5) PORT_NAME("Button 5")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_0) PORT_NAME("Button 10")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_T) PORT_NAME("Button 15")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_G) PORT_NAME("Button 20")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_B) PORT_NAME("Button 25")

	PORT_START("IN.5") // R5
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_M) PORT_NAME("Game Select")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_N) PORT_NAME("Game Start")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END

static MACHINE_CONFIG_START( amaztron, amaztron_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 350000) // RC osc. R=33K?, C=100pf -> ~350kHz
	MCFG_TMS1XXX_READ_K_CB(READ8(amaztron_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(amaztron_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(amaztron_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_amaztron)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Coleco Total Control 4
  * TMS1400NLL MP7334-N2 (die labeled MP7334)
  * 2x2-digit 7seg LED display + 4 LEDs, LED grid display, 1bit sound

  This is a head to head electronic tabletop LED-display sports console.
  One cartridge(Football) was included with the console, the other three were
  sold separately. Gameplay has emphasis on strategy, read the official manual
  on how to play. Remember that you can rotate the view in MESS: rotate left
  for Home(P1) orientation, rotate right for Visitor(P2) orientation.

  Cartridge socket:
  1 N/C
  2 9V+
  3 power switch
  4 K8
  5 K4
  6 K2
  7 K1
  8 R9

  The cartridge connects pin 8 with one of the K-pins.

  Available cartridges:
  - Football    (K8, confirmed)
  - Hockey      (K4?)
  - Soccer      (K2?)
  - Basketball  (K1?)

***************************************************************************/

class tc4_state : public hh_tms1k_state
{
public:
	tc4_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void tc4_state::prepare_display()
{
	m_display_wait = 50;

	// R5,7,8,9 are 7segs
	for (int y = 0; y < 10; y++)
		if (y >= 5 && y <= 9 && y != 6)
			m_display_segmask[y] = 0x7f;

	// update current state (note: R6 as extra column!)
	display_matrix(9, 10, (m_o | (m_r << 2 & 0x100)), m_r);
}

WRITE16_MEMBER(tc4_state::write_r)
{
	// R10: speaker out
	m_speaker->level_w(data >> 10 & 1);

	// R0-R5: input mux
	// R9: to cartridge slot
	m_inp_mux = data & 0x23f;

	// R6: led column 8
	// +other columns
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(tc4_state::write_o)
{
	// O0-O7: led state
	m_o = data;
	prepare_display();
}

READ8_MEMBER(tc4_state::read_k)
{
	UINT8 k = read_inputs(6);

	// read from cartridge
	if (m_inp_mux & 0x200)
		k |= m_inp_matrix[6]->read();

	return k;
}


// config

static INPUT_PORTS_START( tc4 )
	PORT_START("IN.0") // R0
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("P1 Pass/Shoot Button 3") // right
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("P1 Pass/Shoot Button 1") // left
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("P1 Pass/Shoot Button 2") // middle
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_NAME("P1 D/K Button")

	PORT_START("IN.1") // R1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN )

	PORT_START("IN.2") // R2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN )

	PORT_START("IN.3") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN ) PORT_PLAYER(2)

	PORT_START("IN.4") // R4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN ) PORT_PLAYER(2)

	PORT_START("IN.5") // R5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_PLAYER(2) PORT_NAME("P2 Pass/Shoot Button 3") // right
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_NAME("P2 Pass/Shoot Button 1") // left
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(2) PORT_NAME("P2 Pass/Shoot Button 2") // middle
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_PLAYER(2) PORT_NAME("P2 D/K Button")

	PORT_START("IN.6") // R9
	PORT_CONFNAME( 0x0f, 0x08, "Cartridge")
	PORT_CONFSETTING(    0x01, "Basketball" )
	PORT_CONFSETTING(    0x02, "Soccer" )
	PORT_CONFSETTING(    0x04, "Hockey" )
	PORT_CONFSETTING(    0x08, "Football" )
INPUT_PORTS_END

static MACHINE_CONFIG_START( tc4, tc4_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1400, 450000) // approximation - RC osc. R=27.3K, C=100pf, but unknown RC curve
	MCFG_TMS1XXX_READ_K_CB(READ8(tc4_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(tc4_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(tc4_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_tc4)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Entex Electronic Baseball (1)
  * TMS1000NLP MP0914 (die labeled MP0914A)
  * 1 7seg LED, and other LEDs behind bezel, 1bit sound

  This is a handheld LED baseball game. One player controls the batter, the CPU
  or other player controls the pitcher. Pitcher throw buttons are on a 'joypad'
  obtained from a compartment in the back. Player scores are supposed to be
  written down manually, the game doesn't save scores or innings (this annoyance
  was resolved in the sequel). For more information, refer to the official manual.

  The overlay graphic is known to have 2 versions: one where the field players
  are denoted by words ("left", "center", "short", etc), and an alternate one
  with little guys drawn next to the LEDs.


  lamp translation table: led LDzz from game PCB = MESS lampyx:

    LD0  = -        LD10 = lamp12   LD20 = lamp42   LD30 = lamp60
    LD1  = lamp23   LD11 = lamp4    LD21 = lamp41   LD31 = lamp61
    LD2  = lamp0    LD12 = lamp15   LD22 = lamp40   LD32 = lamp62
    LD3  = lamp1    LD13 = lamp22   LD23 = lamp43   LD33 = lamp70
    LD4  = lamp2    LD14 = lamp33   LD24 = lamp53   LD34 = lamp71
    LD5  = lamp10   LD15 = lamp32   LD25 = lamp52
    LD6  = lamp13   LD16 = lamp21   LD26 = lamp51
    LD7  = lamp11   LD17 = lamp31   LD27 = lamp50
    LD8  = lamp3    LD18 = lamp30   LD28 = lamp72
    LD9  = lamp14   LD19 = lamp20   LD29 = lamp73

***************************************************************************/

class ebball_state : public hh_tms1k_state
{
public:
	ebball_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void ebball_state::prepare_display()
{
	// R8 is a 7seg
	m_display_segmask[8] = 0x7f;

	display_matrix(7, 9, ~m_o, m_r);
}

WRITE16_MEMBER(ebball_state::write_r)
{
	// R1-R5: input mux
	m_inp_mux = data >> 1 & 0x1f;

	// R9: speaker out
	m_speaker->level_w(data >> 9 & 1);

	// R0-R8: led select
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(ebball_state::write_o)
{
	// O0-O6: led state
	// O7: N/C
	m_o = data;
	prepare_display();
}

READ8_MEMBER(ebball_state::read_k)
{
	// note: K8(Vss row) is always on
	return m_inp_matrix[5]->read() | read_inputs(5);
}


// config

static INPUT_PORTS_START( ebball )
	PORT_START("IN.0") // R1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(2) PORT_NAME("P2 Change Up")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START ) PORT_NAME("Change Sides")
	PORT_CONFNAME( 0x04, 0x04, "Pitcher" )
	PORT_CONFSETTING(    0x04, "Auto" )
	PORT_CONFSETTING(    0x00, "Manual" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // R2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_NAME("P2 Fast Ball")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_PLAYER(2) PORT_NAME("P2 Knuckler")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // R4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_PLAYER(2) PORT_NAME("P2 Curve")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // R5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_PLAYER(2) PORT_NAME("P2 Slider")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // Vss!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("P1 Batter")
INPUT_PORTS_END

static MACHINE_CONFIG_START( ebball, ebball_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1000, 375000) // RC osc. R=43K, C=47pf -> ~375kHz
	MCFG_TMS1XXX_READ_K_CB(READ8(ebball_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(ebball_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(ebball_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_ebball)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Entex Electronic Baseball 2
  * PCBs are labeled: ZENY
  * TMS1000 MCU, MP0923 (die labeled MP0923)
  * 3 7seg LEDs, and other LEDs behind bezel, 1bit sound

  The Japanese version was published by Gakken, black casing instead of white.

  The sequel to Entex Baseball, this version keeps up with score and innings.
  As its predecessor, the pitcher controls are on a separate joypad.


  lamp translation table: led zz from game PCB = MESS lampyx:

    00 = -        10 = lamp94   20 = lamp74   30 = lamp50
    01 = lamp53   11 = lamp93   21 = lamp75   31 = lamp51
    02 = lamp7    12 = lamp92   22 = lamp80   32 = lamp52
    03 = lamp17   13 = lamp62   23 = lamp81   33 = lamp40
    04 = lamp27   14 = lamp70   24 = lamp82   34 = lamp41
    05 = lamp97   15 = lamp71   25 = lamp83   35 = lamp31
    06 = lamp90   16 = lamp61   26 = lamp84   36 = lamp30
    07 = lamp95   17 = lamp72   27 = lamp85   37 = lamp33
    08 = lamp63   18 = lamp73   28 = lamp42   38 = lamp32
    09 = lamp91   19 = lamp60   29 = lamp43

***************************************************************************/

class ebball2_state : public hh_tms1k_state
{
public:
	ebball2_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void ebball2_state::prepare_display()
{
	// the first 3 are 7segs
	for (int y = 0; y < 3; y++)
		m_display_segmask[y] = 0x7f;

	display_matrix(8, 10, ~m_o, m_r ^ 0x7f);
}

WRITE16_MEMBER(ebball2_state::write_r)
{
	// R3-R6: input mux
	m_inp_mux = data >> 3 & 0xf;

	// R10: speaker out
	m_speaker->level_w(data >> 10 & 1);

	// R0-R9: led select
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(ebball2_state::write_o)
{
	// O0-O7: led state
	m_o = data;
	prepare_display();
}

READ8_MEMBER(ebball2_state::read_k)
{
	return read_inputs(4);
}


// config

static INPUT_PORTS_START( ebball2 )
	PORT_START("IN.0") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_CONFNAME( 0x02, 0x02, "Pitcher" )
	PORT_CONFSETTING(    0x02, "Auto" )
	PORT_CONFSETTING(    0x00, "Manual" )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_NAME("P2 Fast Ball")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("P1 Batter")

	PORT_START("IN.1") // R4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("P1 Steal")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(2) PORT_NAME("P2 Change Up")
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // R5
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_PLAYER(2) PORT_NAME("P2 Slider")
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // R6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_PLAYER(2) PORT_NAME("P2 Knuckler")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_PLAYER(2) PORT_NAME("P2 Curve")
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

static MACHINE_CONFIG_START( ebball2, ebball2_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1000, 350000) // RC osc. R=47K, C=47pf -> ~350kHz
	MCFG_TMS1XXX_READ_K_CB(READ8(ebball2_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(ebball2_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(ebball2_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_ebball2)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Entex Electronic Baseball 3
  * PCBs are labeled: ZENY
  * TMS1100NLL 6007 MP1204 (die labeled MP1204)
  * 2*SN75492N LED display driver
  * 4 7seg LEDs, and other LEDs behind bezel, 1bit sound

  This is another improvement over Entex Baseball, where gameplay is a bit more
  varied. Like the others, the pitcher controls are on a separate joypad.


  lamp translation table: led zz from game PCB = MESS lampyx:
  note: unlabeled panel leds are listed here as Sz, Bz, Oz, Iz, z left-to-right

    00 = -        10 = lamp75   20 = lamp72
    01 = lamp60   11 = lamp65   21 = lamp84
    02 = lamp61   12 = lamp55   22 = lamp85
    03 = lamp62   13 = lamp52   23 = lamp90
    04 = lamp63   14 = lamp80   24 = lamp92
    05 = lamp73   15 = lamp81   25 = lamp91
    06 = lamp53   16 = lamp51   26 = lamp93
    07 = lamp74   17 = lamp82   27 = lamp95
    08 = lamp64   18 = lamp83   28 = lamp94
    09 = lamp54   19 = lamp50

    S1,S2: lamp40,41
    B1-B3: lamp30-32
    O1,O2: lamp42,43
    I1-I6: lamp20-25, I7-I9: lamp33-35

***************************************************************************/

class ebball3_state : public hh_tms1k_state
{
public:
	ebball3_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);

	void set_clock();
	DECLARE_INPUT_CHANGED_MEMBER(difficulty_switch);

protected:
	virtual void machine_reset();
};

// handlers

void ebball3_state::prepare_display()
{
	// update current state
	for (int y = 0; y < 10; y++)
		m_display_state[y] = (m_r >> y & 1) ? m_o : 0;

	// R0,R1 are normal 7segs
	m_display_segmask[0] = m_display_segmask[1] = 0x7f;

	// R4,R7 contain segments(only F and B) for the two other digits
	m_display_state[10] = (m_display_state[4] & 0x20) | (m_display_state[7] & 0x02);
	m_display_state[11] = ((m_display_state[4] & 0x10) | (m_display_state[7] & 0x01)) << 1;
	m_display_segmask[10] = m_display_segmask[11] = 0x22;

	set_display_size(7, 10+2);
	display_update();
}

WRITE16_MEMBER(ebball3_state::write_r)
{
	// R0-R2: input mux
	m_inp_mux = data & 7;

	// R10: speaker out
	m_speaker->level_w(data >> 10 & 1);

	// R0-R9: led select
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(ebball3_state::write_o)
{
	// O0-O6: led state
	// O7: N/C
	m_o = data & 0x7f;
	prepare_display();
}

READ8_MEMBER(ebball3_state::read_k)
{
	return read_inputs(3);
}


// config

/* physical button layout and labels is like this:

    main device (batter side):            remote pitcher:

                          MAN
    PRO                    |              [FAST BALL]  [CHANGE UP]    [CURVE]  [SLIDER]
     |                  OFF|
     o                     o                   [STEAL DEFENSE]           [KNUCKLER]
    AM                    AUTO

    [BUNT]  [BATTER]  [STEAL]
*/

static INPUT_PORTS_START( ebball3 )
	PORT_START("IN.0") // R0
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_NAME("P2 Fast Ball")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(2) PORT_NAME("P2 Change Up")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_PLAYER(2) PORT_NAME("P2 Slider")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_PLAYER(2) PORT_NAME("P2 Curve")

	PORT_START("IN.1") // R1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON6 ) PORT_PLAYER(2) PORT_NAME("P2 Knuckler")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("P1 Steal")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("P1 Batter")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_PLAYER(2) PORT_NAME("P2 Steal Defense")

	PORT_START("IN.2") // R2
	PORT_CONFNAME( 0x01, 0x01, "Pitcher" )
	PORT_CONFSETTING(    0x01, "Auto" )
	PORT_CONFSETTING(    0x00, "Manual" )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("P1 Bunt")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // fake
	PORT_CONFNAME( 0x01, 0x00, DEF_STR( Difficulty ) ) PORT_CHANGED_MEMBER(DEVICE_SELF, ebball3_state, difficulty_switch, NULL)
	PORT_CONFSETTING(    0x00, "Amateur" )
	PORT_CONFSETTING(    0x01, "Professional" )
INPUT_PORTS_END

INPUT_CHANGED_MEMBER(ebball3_state::difficulty_switch)
{
	set_clock();
}


void ebball3_state::set_clock()
{
	// MCU clock is from an RC circuit(R=47K, C=33pf) oscillating by default at ~340kHz,
	// but on PRO, the difficulty switch adds an extra 150K resistor to Vdd to speed
	// it up to around ~440kHz.
	m_maincpu->set_unscaled_clock((m_inp_matrix[3]->read() & 1) ? 440000 : 340000);
}

void ebball3_state::machine_reset()
{
	hh_tms1k_state::machine_reset();
	set_clock();
}

static MACHINE_CONFIG_START( ebball3, ebball3_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 340000) // see set_clock
	MCFG_TMS1XXX_READ_K_CB(READ8(ebball3_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(ebball3_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(ebball3_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_ebball3)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Entex Space Invader
  * TMS1100 MP1211 (die labeled MP1211)
  * 3 7seg LEDs, LED matrix and overlay mask, 1bit sound

  There are two versions of this game: the first release(this one) is on
  TMS1100, the second more widespread release runs on a COP400. There are
  also differences with the overlay mask.

  NOTE!: MESS external artwork is recommended

***************************************************************************/

class einvader_state : public hh_tms1k_state
{
public:
	einvader_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);

	void set_clock();
	DECLARE_INPUT_CHANGED_MEMBER(difficulty_switch);

protected:
	virtual void machine_reset();
};

// handlers

void einvader_state::prepare_display()
{
	// R7-R9 are 7segs
	for (int y = 7; y < 10; y++)
		m_display_segmask[y] = 0x7f;
	
	display_matrix(8, 10, m_o, m_r);
}

WRITE16_MEMBER(einvader_state::write_r)
{
	// R10: speaker out
	m_speaker->level_w(data >> 10 & 1);

	// R0-R9: led select
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(einvader_state::write_o)
{
	// O0-O7: led state
	m_o = data;
	prepare_display();
}


// config

static INPUT_PORTS_START( einvader )
	PORT_START("IN.0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  ) PORT_16WAY // separate directional buttons, hence 16way
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_16WAY // "
	PORT_CONFNAME( 0x08, 0x00, DEF_STR( Difficulty ) ) PORT_CHANGED_MEMBER(DEVICE_SELF, einvader_state, difficulty_switch, NULL)
	PORT_CONFSETTING(    0x00, "Amateur" )
	PORT_CONFSETTING(    0x08, "Professional" )
INPUT_PORTS_END

INPUT_CHANGED_MEMBER(einvader_state::difficulty_switch)
{
	set_clock();
}


void einvader_state::set_clock()
{
	// MCU clock is from an RC circuit(R=47K, C=56pf) oscillating by default at ~320kHz,
	// but on PRO, the difficulty switch adds an extra 180K resistor to Vdd to speed
	// it up to around ~400kHz.
	m_maincpu->set_unscaled_clock((m_inp_matrix[0]->read() & 8) ? 400000 : 320000);
}

void einvader_state::machine_reset()
{
	hh_tms1k_state::machine_reset();
	set_clock();
}

static MACHINE_CONFIG_START( einvader, einvader_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 320000) // see set_clock
	MCFG_TMS1XXX_READ_K_CB(IOPORT("IN.0"))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(einvader_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(einvader_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_einvader)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Ideal Electronic Detective
  * TMS0980NLL MP6100A (die labeled 0980B-00)
  * 10-digit 7seg LED display, 1bit sound

  hardware (and concept) is very similar to Parker Bros Stop Thief

  This is an electronic board game. It requires game cards with suspect info,
  and good old pen and paper to record game progress. To start the game, enter
  difficulty(1-3), then number of players(1-4), then [ENTER]. Refer to the
  manual for more information.

***************************************************************************/

class elecdet_state : public hh_tms1k_state
{
public:
	elecdet_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(elecdet_state::write_r)
{
	// R7,R8: speaker on
	m_speaker->level_w((data & 0x180 && m_o & 0x80) ? 1 : 0);

	// R0-R6: select digit
	for (int y = 0; y < 7; y++)
		m_display_segmask[y] = 0x7f;

	display_matrix(7, 7, BITSWAP8(m_o,7,5,2,1,4,0,6,3), data);
}

WRITE16_MEMBER(elecdet_state::write_o)
{
	// O0,O1,O4,O6: input mux
	m_inp_mux = (data & 3) | (data >> 2 & 4) | (data >> 3 & 8);

	// O0-O6: led segments A-G
	// O7: speaker out
	m_o = data;
}

READ8_MEMBER(elecdet_state::read_k)
{
	// note: the Vss row is always on
	return m_inp_matrix[4]->read() | read_inputs(4);
}


// config

/* physical button layout and labels is like this:

    [1]   [2]   [3]   [SUSPECT]
    [4]   [5]   [6]   [PRIVATE QUESTION]
    [7]   [8]   [9]   [I ACCUSE]
                [0]   [ENTER]
    [ON]  [OFF]       [END TURN]
*/

static INPUT_PORTS_START( elecdet )
	PORT_START("IN.0") // O0
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_Q) PORT_NAME("Private Question")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")

	PORT_START("IN.1") // O1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("0")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("Enter")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // O4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_A) PORT_NAME("I Accuse")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")

	PORT_START("IN.3") // O6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_S) PORT_NAME("Suspect")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")

	// note: even though power buttons are on the matrix, they are not CPU-controlled
	PORT_START("IN.4") // Vss!
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_PGUP) PORT_NAME("On") PORT_CHANGED_MEMBER(DEVICE_SELF, hh_tms1k_state, power_button, (void *)true)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_E) PORT_NAME("End Turn")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_PGDN) PORT_NAME("Off") PORT_CHANGED_MEMBER(DEVICE_SELF, hh_tms1k_state, power_button, (void *)false)
INPUT_PORTS_END

static MACHINE_CONFIG_START( elecdet, elecdet_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS0980, 425000) // approximation - unknown freq
	MCFG_TMS1XXX_READ_K_CB(READ8(elecdet_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(elecdet_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(elecdet_state, write_o))
	MCFG_TMS1XXX_POWER_OFF_CB(WRITELINE(hh_tms1k_state, auto_power_off))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_elecdet)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Kenner Star Wars - Electronic Battle Command
  * TMS1100 MCU, labeled MP3438A
  * 4x4 LED grid display + 2 separate LEDs and 2-digit 7segs, 1bit sound

  This is a small tabletop space-dogfighting game. To start the game,
  press BASIC/INTER/ADV and enter P#(number of players), then
  START TURN. Refer to the official manual for more information.

***************************************************************************/

class starwbc_state : public hh_tms1k_state
{
public:
	starwbc_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void starwbc_state::prepare_display()
{
	// R6,R8 are 7segs
	m_display_segmask[6] = m_display_segmask[8] = 0x7f;
	display_matrix(8, 10, m_o, m_r);
}

WRITE16_MEMBER(starwbc_state::write_r)
{
	// R0,R1,R3,R5,R7: input mux
	m_inp_mux = (data & 3) | (data >> 1 & 4) | (data >> 2 & 8) | (data >> 3 & 0x10);

	// R9: speaker out
	m_speaker->level_w(data >> 9 & 1);

	// R0,R2,R4,R6,R8: led select
	m_r = data & 0x155;
	prepare_display();
}

WRITE16_MEMBER(starwbc_state::write_o)
{
	// O0-O7: led state
	m_o = (data << 4 & 0xf0) | (data >> 4 & 0x0f);
	prepare_display();
}

READ8_MEMBER(starwbc_state::read_k)
{
	return read_inputs(5);
}


// config

/* physical button layout and labels is like this:

    (reconnnaissance=yellow)        (tactical reaction=green)
    [MAGNA] [ENEMY]                 [EM]       [BS]   [SCR]

    [BASIC] [INTER]    [START TURN] [END TURN] [MOVE] [FIRE]
    [ADV]   [P#]       [<]          [^]        [>]    [v]
    (game=blue)        (maneuvers=red)
*/

static INPUT_PORTS_START( starwbc )
	PORT_START("IN.0") // R0
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_1) PORT_NAME("Basic Game")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_2) PORT_NAME("Intermediate Game")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_3) PORT_NAME("Advanced Game")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_N) PORT_NAME("Player Number")

	PORT_START("IN.1") // R1
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_ENTER) PORT_NAME("Start Turn")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL) PORT_NAME("End Turn")

	PORT_START("IN.2") // R3
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_Q) PORT_NAME("Magna Scan") // only used in adv. game
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_W) PORT_NAME("Enemy Scan") // only used in adv. game
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_S) PORT_NAME("Screen Up")

	PORT_START("IN.3") // R5
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_E) PORT_NAME("Evasive Maneuvers")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_M) PORT_NAME("Move")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F) PORT_NAME("Fire")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_B) PORT_NAME("Battle Stations")

	PORT_START("IN.4") // R7
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_LEFT) PORT_NAME("Left")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_UP) PORT_NAME("Up")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_DOWN) PORT_NAME("Down")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_RIGHT) PORT_NAME("Right")
INPUT_PORTS_END

static MACHINE_CONFIG_START( starwbc, starwbc_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 325000) // RC osc. R=51K, C=47pf -> ~325kHz
	MCFG_TMS1XXX_READ_K_CB(READ8(starwbc_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(starwbc_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(starwbc_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_starwbc)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Kosmos Astro
  * TMS1470NLHL MP1133 (die labeled TMS1400 MP1133)
  * 9digit 7seg VFD display + 8 LEDs(4 green, 4 yellow), no sound

  This is an astrological calculator, and also supports 4-function
  calculations. Refer to the official manual on how to use this device.

***************************************************************************/

class astro_state : public hh_tms1k_state
{
public:
	astro_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void astro_state::prepare_display()
{
	// declare 7segs
	for (int y = 0; y < 9; y++)
		m_display_segmask[y] = 0xff;

	display_matrix(8, 10, m_o, m_r);
}

WRITE16_MEMBER(astro_state::write_r)
{
	// R0-R7: input mux
	m_inp_mux = data & 0xff;

	// R0-R9: led select
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(astro_state::write_o)
{
	// O0-O7: led state
	m_o = data;
	prepare_display();
}

READ8_MEMBER(astro_state::read_k)
{
	return read_inputs(8);
}


// config

static INPUT_PORTS_START( astro )
	PORT_START("IN.0") // R0
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("0")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_SLASH_PAD) PORT_NAME(UTF8_DIVIDE"/Sun")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // R1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_ASTERISK) PORT_NAME(UTF8_MULTIPLY"/Mercury")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // R2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_MINUS_PAD) PORT_NAME("-/Venus")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_PLUS_PAD) PORT_NAME("+/Mars")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // R4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("=/Astro")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // R5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Z) PORT_NAME("B1")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_X) PORT_NAME("B2")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_STOP) PORT_CODE(KEYCODE_DEL_PAD) PORT_NAME(".")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // R6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_DEL) PORT_NAME("C")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.7") // R7
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_CONFNAME( 0x08, 0x08, "Mode" )
	PORT_CONFSETTING(    0x00, "Calculator" )
	PORT_CONFSETTING(    0x08, "Astro" )
INPUT_PORTS_END

static MACHINE_CONFIG_START( astro, astro_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1470, 450000) // approximation - RC osc. R=4.7K, C=33pf, but unknown RC curve
	MCFG_TMS1XXX_READ_K_CB(READ8(astro_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(astro_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(astro_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_astro)

	/* no video! */

	/* no sound! */
MACHINE_CONFIG_END





/***************************************************************************

  Milton Bradley Comp IV
  * TMC0904NL CP0904A (die labeled 4A0970D-04A)
  * 10 LEDs behind bezel, no sound

  This is small tabletop Mastermind game; a code-breaking game where the player
  needs to find out the correct sequence of colours (numbers in our case).
  It is known as Logic 5 in Europe, and as Pythaligoras in Japan.

  Press the R key to start, followed by a set of unique numbers and E.
  Refer to the official manual for more information.

***************************************************************************/

class comp4_state : public hh_tms1k_state
{
public:
	comp4_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(comp4_state::write_r)
{
	// leds:
	// R4    R9
	// R10!  R8
	// R2    R7
	// R1    R6
	// R0    R5
	m_r = data;
	display_matrix(11, 1, m_r, m_o);
}

WRITE16_MEMBER(comp4_state::write_o)
{
	// O1-O3: input mux
	m_inp_mux = data >> 1 & 7;

	// O0: leds common
	// other bits: N/C
	m_o = data;
	display_matrix(11, 1, m_r, m_o);
}

READ8_MEMBER(comp4_state::read_k)
{
	return read_inputs(3);
}


// config

static INPUT_PORTS_START( comp4 )
	PORT_START("IN.0") // O1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_R) PORT_CODE(KEYCODE_DEL) PORT_CODE(KEYCODE_DEL_PAD) PORT_NAME("R")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")

	PORT_START("IN.1") // O2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("0")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")

	PORT_START("IN.2") // O3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_E) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("E")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9")
INPUT_PORTS_END

static MACHINE_CONFIG_START( comp4, comp4_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS0970, 250000) // approximation - unknown freq
	MCFG_TMS1XXX_READ_K_CB(READ8(comp4_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(comp4_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(comp4_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_comp4)

	/* no video! */

	/* no sound! */
MACHINE_CONFIG_END





/***************************************************************************

  Milton Bradley Simon, created by Ralph Baer

  Revision A hardware:
  * TMS1000 (die labeled MP3226)
  * DS75494 lamp driver, 4 big lamps, 1bit sound

  Newer revisions (also Pocket Simon) have a smaller 16-pin MB4850 chip
  instead of the TMS1000. This one has been decapped too, but we couldn't
  find an internal ROM. It is possibly a cost-reduced custom ASIC specifically
  for Simon. The semi-sequel Super Simon uses a TMS1100 (see next minidriver).

***************************************************************************/

class simon_state : public hh_tms1k_state
{
public:
	simon_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(simon_state::write_r)
{
	// R4-R8 go through an 75494 IC first:
	// R4 -> 75494 IN6 -> green lamp
	// R5 -> 75494 IN3 -> red lamp
	// R6 -> 75494 IN5 -> yellow lamp
	// R7 -> 75494 IN2 -> blue lamp
	display_matrix(4, 1, data >> 4, 1);

	// R8 -> 75494 IN0 -> speaker out
	m_speaker->level_w(data >> 8 & 1);

	// R0-R2,R9: input mux
	// R3: GND
	// other bits: N/C
	m_inp_mux = (data & 7) | (data >> 6 & 8);
}

READ8_MEMBER(simon_state::read_k)
{
	return read_inputs(4);
}


// config

static INPUT_PORTS_START( simon )
	PORT_START("IN.0") // R0
	PORT_CONFNAME( 0x07, 0x02, "Game Select")
	PORT_CONFSETTING(    0x02, "1" )
	PORT_CONFSETTING(    0x01, "2" )
	PORT_CONFSETTING(    0x04, "3" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // R1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Green Button")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("Red Button")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("Yellow Button")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_NAME("Blue Button")

	PORT_START("IN.2") // R2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_NAME("Last")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON6 ) PORT_NAME("Longest")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // R9
	PORT_CONFNAME( 0x0f, 0x02, "Skill Level")
	PORT_CONFSETTING(    0x02, "1" )
	PORT_CONFSETTING(    0x04, "2" )
	PORT_CONFSETTING(    0x08, "3" )
	PORT_CONFSETTING(    0x01, "4" )
INPUT_PORTS_END

static MACHINE_CONFIG_START( simon, simon_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1000, 350000) // RC osc. R=33K, C=100pf -> ~350kHz
	MCFG_TMS1XXX_READ_K_CB(READ8(simon_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(simon_state, write_r))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_simon)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Milton Bradley Super Simon
  * TMS1100 MP3476NLL (die labeled MP3476)
  * 8 big lamps(2 turn on at same time), 1bit sound

  The semi-squel to Simon, not as popular. It includes more game variations
  and a 2-player head-to-head mode.

***************************************************************************/

class ssimon_state : public hh_tms1k_state
{
public:
	ssimon_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_READ8_MEMBER(read_k);

	void set_clock();
	DECLARE_INPUT_CHANGED_MEMBER(speed_switch);

protected:
	virtual void machine_reset();
};

// handlers

WRITE16_MEMBER(ssimon_state::write_r)
{
	// R0-R3,R9,R10: input mux
	m_inp_mux = (data & 0xf) | (data >> 5 & 0x30);

	// R4: yellow lamps
	// R5: green lamps
	// R6: blue lamps
	// R7: red lamps
	display_matrix(4, 1, data >> 4, 1);

	// R8: speaker out
	m_speaker->level_w(data >> 8 & 1);
}

READ8_MEMBER(ssimon_state::read_k)
{
	return read_inputs(6);
}


// config

static INPUT_PORTS_START( ssimon )
	PORT_START("IN.0") // R0
	PORT_CONFNAME( 0x0f, 0x01, "Game Select")
	PORT_CONFSETTING(    0x01, "1" )
	PORT_CONFSETTING(    0x02, "2" )
	PORT_CONFSETTING(    0x04, "3" )
	PORT_CONFSETTING(    0x08, "4" )
	PORT_CONFSETTING(    0x00, "5" )

	PORT_START("IN.1") // R1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_PLAYER(2) PORT_NAME("P2 Yellow Button")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_PLAYER(2) PORT_NAME("P2 Green Button")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(2) PORT_NAME("P2 Blue Button")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_NAME("P2 Red Button")

	PORT_START("IN.2") // R2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_NAME("Last")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON6 ) PORT_NAME("Longest")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON7 ) PORT_NAME("Decision")

	PORT_START("IN.3") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_NAME("P1 Yellow Button")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("P1 Green Button")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("P1 Blue Button")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("P1 Red Button")

	PORT_START("IN.4") // R9
	PORT_CONFNAME( 0x0f, 0x02, "Skill Level")
	PORT_CONFSETTING(    0x00, "Head-to-Head" ) // this sets R10 K2, see below
	PORT_CONFSETTING(    0x02, "1" )
	PORT_CONFSETTING(    0x04, "2" )
	PORT_CONFSETTING(    0x08, "3" )
	PORT_CONFSETTING(    0x01, "4" )

	PORT_START("IN.5") // R10
	PORT_BIT( 0x02, 0x02, IPT_SPECIAL ) PORT_CONDITION("IN.4", 0x0f, EQUALS, 0x00)
	PORT_BIT( 0x02, 0x00, IPT_SPECIAL ) PORT_CONDITION("IN.4", 0x0f, NOTEQUALS, 0x00)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // fake
	PORT_CONFNAME( 0x03, 0x01, "Speed" ) PORT_CHANGED_MEMBER(DEVICE_SELF, ssimon_state, speed_switch, NULL)
	PORT_CONFSETTING(    0x00, "Simple" )
	PORT_CONFSETTING(    0x01, "Normal" )
	PORT_CONFSETTING(    0x02, "Super" )
INPUT_PORTS_END

INPUT_CHANGED_MEMBER(ssimon_state::speed_switch)
{
	set_clock();
}


void ssimon_state::set_clock()
{
	// MCU clock is from an RC circuit with C=100pf, R=x depending on speed switch:
	// 0 Simple: R=51K -> ~200kHz
	// 1 Normal: R=37K -> ~275kHz
	// 2 Super:  R=22K -> ~400kHz
	UINT8 inp = m_inp_matrix[6]->read();
	m_maincpu->set_unscaled_clock((inp & 2) ? 400000 : ((inp & 1) ? 275000 : 200000));
}

void ssimon_state::machine_reset()
{
	hh_tms1k_state::machine_reset();
	set_clock();
}

static MACHINE_CONFIG_START( ssimon, ssimon_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 275000) // see set_clock
	MCFG_TMS1XXX_READ_K_CB(READ8(ssimon_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(ssimon_state, write_r))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_ssimon)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Parker Brothers Code Name: Sector, by Bob Doyle
  * TMS0970 MCU, MP0905BNL ZA0379 (die labeled 0970F-05B)
  * 6-digit 7seg LED display + 4 LEDs for compass, no sound

  This is a tabletop submarine pursuit game. A grid board and small toy
  boats are used to remember your locations (a Paint app should be ok too).
  Refer to the official manual for more information, it is not a simple game.

***************************************************************************/

class cnsector_state : public hh_tms1k_state
{
public:
	cnsector_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(cnsector_state::write_r)
{
	// R0-R5: select digit (right-to-left)
	for (int y = 0; y < 6; y++)
	{
		m_display_segmask[y] = 0xff;
		m_display_state[y] = (data >> y & 1) ? m_o : 0;
	}

	// R6-R9: direction leds (-> lamp60-63)
	m_display_state[6] = data >> 6 & 0xf;

	set_display_size(8, 7);
	display_update();
}

WRITE16_MEMBER(cnsector_state::write_o)
{
	// O0-O4: input mux
	m_inp_mux = data & 0x1f;

	// O0-O7: digit segments
	m_o = data;
}

READ8_MEMBER(cnsector_state::read_k)
{
	return read_inputs(5);
}


// config

static INPUT_PORTS_START( cnsector )
	PORT_START("IN.0") // O0
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_Q) PORT_NAME("Next Ship")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_A) PORT_NAME("Left")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_Z) PORT_NAME("Range")

	PORT_START("IN.1") // O1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_X) PORT_NAME("Aim")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_S) PORT_NAME("Right")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // O2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_C) PORT_NAME("Fire")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_J) PORT_NAME("Evasive Sub") // expert button
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_E) PORT_NAME("Recall")

	PORT_START("IN.3") // O3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_M) PORT_NAME("Sub Finder") // expert button
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_F) PORT_NAME("Slower")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // O4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_B) PORT_NAME("Teach Mode")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_G) PORT_NAME("Faster")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_T) PORT_NAME("Move Ship")
INPUT_PORTS_END

static MACHINE_CONFIG_START( cnsector, cnsector_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS0970, 250000) // approximation - unknown freq
	MCFG_TMS1XXX_READ_K_CB(READ8(cnsector_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(cnsector_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(cnsector_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_cnsector)

	/* no video! */

	/* no sound! */
MACHINE_CONFIG_END





/***************************************************************************

  Parker Bros Merlin handheld game, by Bob Doyle
  * TMS1100NLL MP3404A-N2
  * 11 LEDs behind buttons, 2bit sound

  Also published in Japan by Tomy as "Dr. Smith", white case instead of red.
  The one with dark-blue case is the rare sequel Master Merlin. More sequels
  followed too, but on other hardware.

  To start a game, press NEW GAME, followed by a number:
  1: Tic-Tac-Toe
  2: Music Machine
  3: Echo
  4: Blackjack 13
  5: Magic Square
  6: Mindbender

  Refer to the official manual for more information on the games.

***************************************************************************/

class merlin_state : public hh_tms1k_state
{
public:
	merlin_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(merlin_state::write_r)
{
	/* leds:

	     R0
	R1   R2   R3
	R4   R5   R6
	R7   R8   R9
	     R10
	*/
	display_matrix(11, 1, data, 1);
}

WRITE16_MEMBER(merlin_state::write_o)
{
	// O4-O6: speaker out (paralleled for increased current driving capability)
	static const int count[8] = { 0, 1, 1, 2, 1, 2, 2, 3 };
	m_speaker->level_w(count[data >> 4 & 7]);

	// O0-O3: input mux
	// O7: N/C
	m_inp_mux = data & 0xf;
}

READ8_MEMBER(merlin_state::read_k)
{
	return read_inputs(4);
}


// config

static INPUT_PORTS_START( merlin )
	PORT_START("IN.0") // O0
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_SLASH_PAD) PORT_NAME("Button 0")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("Button 1")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("Button 3")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("Button 2")

	PORT_START("IN.1") // O1
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("Button 4")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("Button 5")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("Button 7")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("Button 6")

	PORT_START("IN.2") // O2
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("Button 8")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("Button 9")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_S) PORT_NAME("Same Game")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_MINUS) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("Button 10")

	PORT_START("IN.3") // O3
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_C) PORT_NAME("Comp Turn")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_H) PORT_NAME("Hit Me")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_N) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("New Game")
INPUT_PORTS_END


static const INT16 merlin_speaker_levels[] = { 0, 10922, 21845, 32767 };

static MACHINE_CONFIG_START( merlin, merlin_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 350000) // RC osc. R=33K, C=100pf -> ~350kHz
	MCFG_TMS1XXX_READ_K_CB(READ8(merlin_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(merlin_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(merlin_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_merlin)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SPEAKER_LEVELS(4, merlin_speaker_levels)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Parker Brothers Stop Thief, by Bob Doyle
  * TMS0980NLL MP6101B (die labeled 0980B-01A)
  * 3-digit 7seg LED display, 1bit sound

  Stop Thief is actually a board game, the electronic device emulated here
  (called Electronic Crime Scanner) is an accessory. To start a game, press
  the ON button. Otherwise, it is in test-mode where you can hear all sounds.

***************************************************************************/

class stopthief_state : public hh_tms1k_state
{
public:
	stopthief_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(stopthief_state::write_r)
{
	// R0-R2: select digit
	UINT8 o = BITSWAP8(m_o,3,5,2,1,4,0,6,7) & 0x7f;
	for (int y = 0; y < 3; y++)
	{
		m_display_segmask[y] = 0x7f;
		m_display_state[y] = (data >> y & 1) ? o : 0;
	}

	set_display_size(7, 3);
	display_update();

	// R3-R8: speaker on
	m_speaker->level_w((data & 0x1f8 && m_o & 8) ? 1 : 0);
}

WRITE16_MEMBER(stopthief_state::write_o)
{
	// O0,O6: input mux
	m_inp_mux = (data & 1) | (data >> 5 & 2);

	// O3: speaker out
	// O0-O2,O4-O7: led segments A-G
	m_o = data;
}

READ8_MEMBER(stopthief_state::read_k)
{
	// note: the Vss row is always on
	return m_inp_matrix[2]->read() | read_inputs(2);
}


// config

/* physical button layout and labels is like this:

    [1] [2] [OFF]
    [3] [4] [ON]
    [5] [6] [T, TIP]
    [7] [8] [A, ARREST]
    [9] [0] [C, CLUE]
*/

static INPUT_PORTS_START( stopthief )
	PORT_START("IN.0") // O0
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("0")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")

	PORT_START("IN.1") // O6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")

	// note: even though power buttons are on the matrix, they are not CPU-controlled
	PORT_START("IN.2") // Vss!
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_PGUP) PORT_NAME("On") PORT_CHANGED_MEMBER(DEVICE_SELF, hh_tms1k_state, power_button, (void *)true)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_T) PORT_NAME("Tip")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_A) PORT_NAME("Arrest")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_C) PORT_NAME("Clue")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_PGDN) PORT_NAME("Off") PORT_CHANGED_MEMBER(DEVICE_SELF, hh_tms1k_state, power_button, (void *)false)
INPUT_PORTS_END

static MACHINE_CONFIG_START( stopthief, stopthief_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS0980, 425000) // approximation - unknown freq
	MCFG_TMS1XXX_READ_K_CB(READ8(stopthief_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(stopthief_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(stopthief_state, write_o))
	MCFG_TMS1XXX_POWER_OFF_CB(WRITELINE(hh_tms1k_state, auto_power_off))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_stopthie)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Parker Brothers Bank Shot (known as Cue Ball in the UK), by Garry Kitchen
  * TMS1400NLL MP7313-N2 (die labeled MP7313)
  * LED grid display, 1bit sound

  Bank Shot is an electronic pool game. To select a game, repeatedly press
  the [SELECT] button, then press [CUE UP] to start. Refer to the official
  manual for more information. The game selections are:
  1: Straight Pool (1 player)
  2: Straight Pool (2 players)
  3: Poison Pool
  4: Trick Shots

***************************************************************************/

class bankshot_state : public hh_tms1k_state
{
public:
	bankshot_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(bankshot_state::write_r)
{
	// R0: speaker out
	m_speaker->level_w(data & 1);

	// R2,R3: input mux
	m_inp_mux = data >> 2 & 3;

	// R2-R10: led select
	m_r = data & ~3;
	display_matrix(7, 11, m_o, m_r);
}

WRITE16_MEMBER(bankshot_state::write_o)
{
	// O0-O6: led state
	// O7: N/C
	m_o = data;
	display_matrix(7, 11, m_o, m_r);
}

READ8_MEMBER(bankshot_state::read_k)
{
	return read_inputs(2);
}


// config

/* physical button layout and labels is like this:
  (note: remember that you can rotate the display in MESS)

    [SELECT  [BALL UP] [BALL OVER]
     SCORE]

    ------  led display  ------

    [ANGLE]  [AIM]     [CUE UP
                        SHOOT]
*/

static INPUT_PORTS_START( bankshot )
	PORT_START("IN.0") // R2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Angle")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_NAME("Aim")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_NAME("Cue Up / Shoot")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_NAME("Select / Score")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_NAME("Ball Up")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON6 ) PORT_NAME("Ball Over")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

static MACHINE_CONFIG_START( bankshot, bankshot_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1400, 475000) // approximation - RC osc. R=24K, C=100pf, but unknown RC curve
	MCFG_TMS1XXX_READ_K_CB(READ8(bankshot_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(bankshot_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(bankshot_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_bankshot)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Parker Brothers Split Second
  * TMS1400NLL MP7314-N2 (die labeled MP7314)
  * LED grid display(default round LEDs, and rectangular shape ones), 1bit sound

  This is an electronic handheld reflex gaming device, it's straightforward
  to use. The included mini-games are:
  1, 2, 3: Mad Maze*
  4, 5: Space Attack*
  6: Auto Cross
  7: Stomp
  8: Speedball

  *: higher number indicates higher difficulty

  display layout, where number xy is lamp R(x),O(y)

       00    02    04
    10 01 12 03 14 05 16
       11    13    15
    20 21 22 23 24 25 26
       31    33    35
    30 41 32 43 34 45 36
       51    53    55
    40 61 42 63 44 65 46
       71    73    75
    50 60 52 62 54 64 56
       70    72    74

***************************************************************************/

class splitsec_state : public hh_tms1k_state
{
public:
	splitsec_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

WRITE16_MEMBER(splitsec_state::write_r)
{
	// R8: speaker out
	m_speaker->level_w(data >> 8 & 1);

	// R9,R10: input mux
	m_inp_mux = data >> 9 & 3;

	// R0-R7: led select
	m_r = data;
	display_matrix(7, 8, m_o, m_r);
}

WRITE16_MEMBER(splitsec_state::write_o)
{
	// O0-O6: led state
	// O7: N/C
	m_o = data;
	display_matrix(7, 8, m_o, m_r);
}

READ8_MEMBER(splitsec_state::read_k)
{
	return read_inputs(2);
}


// config

static INPUT_PORTS_START( splitsec )
	PORT_START("IN.0") // R9
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_16WAY // 4 separate directional buttons, hence 16way
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_16WAY
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_16WAY
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // R10
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_16WAY
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SELECT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

static MACHINE_CONFIG_START( splitsec, splitsec_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1400, 475000) // approximation - RC osc. R=24K, C=100pf, but unknown RC curve
	MCFG_TMS1XXX_READ_K_CB(READ8(splitsec_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(splitsec_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(splitsec_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_splitsec)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tandy Radio Shack Computerized Arcade (1981, 1982, 1995)
  * TMS1100 MCU, labeled CD7282SL
  * 12 lamps behind buttons, 1bit sound

  This handheld contains 12 minigames. It looks and plays like "Fabulous Fred"
  by the Japanese company Mego Corp. in 1980, which in turn is a mix of Merlin
  and Simon. Unlike Merlin and Simon, spin-offs like these were not successful.
  There were releases with and without the prefix "Tandy-12", I don't know
  which name was more common. Also not worth noting is that it needed five
  batteries; 4 C-cells and a 9-volt.

  Some of the games require accessories included with the toy (eg. the Baseball
  game is played with a board representing the playing field). To start a game,
  hold the [SELECT] button, then press [START] when the game button lights up.
  As always, refer to the official manual for more information.

  See below at the input defs for a list of the games.

***************************************************************************/

class tandy12_state : public hh_tms1k_state
{
public:
	tandy12_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_tms1k_state(mconfig, type, tag)
	{ }

	void prepare_display();
	DECLARE_WRITE16_MEMBER(write_r);
	DECLARE_WRITE16_MEMBER(write_o);
	DECLARE_READ8_MEMBER(read_k);
};

// handlers

void tandy12_state::prepare_display()
{
	// O0-O7: button lamps 1-8, R0-R3: button lamps 9-12
	display_matrix(13, 1, (m_o << 1 & 0x1fe) | (m_r << 9 & 0x1e00), 1);
}

WRITE16_MEMBER(tandy12_state::write_r)
{
	// R10: speaker out
	m_speaker->level_w(data >> 10 & 1);

	// R5-R9: input mux
	m_inp_mux = data >> 5 & 0x1f;

	// other bits:
	m_r = data;
	prepare_display();
}

WRITE16_MEMBER(tandy12_state::write_o)
{
	m_o = data;
	prepare_display();
}

READ8_MEMBER(tandy12_state::read_k)
{
	return read_inputs(5);
}


// config

/* physical button layout and labels is like this:

        REPEAT-2              SPACE-2
          [O]     OFF--ON       [O]

    [purple]1     [blue]5       [l-green]9
    ORGAN         TAG-IT        TREASURE HUNT

    [l-orange]2   [turquoise]6  [red]10
    SONG WRITER   ROULETTE      COMPETE

    [pink]3       [yellow]7     [violet]11
    REPEAT        BASEBALL      FIRE AWAY

    [green]4      [orange]8     [brown]12
    TORPEDO       REPEAT PLUS   HIDE 'N SEEK

          [O]        [O]        [O]
         START      SELECT    PLAY-2/HIT-7
*/

static INPUT_PORTS_START( tandy12 )
	PORT_START("IN.0") // R5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_EQUALS) PORT_CODE(KEYCODE_PLUS_PAD) PORT_NAME("12")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_MINUS) PORT_CODE(KEYCODE_MINUS_PAD) PORT_NAME("11")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("10")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9")

	PORT_START("IN.1") // R6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_T) PORT_NAME("Space-2")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_E) PORT_NAME("Play-2/Hit-7")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_W) PORT_NAME("Select")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_Q) PORT_NAME("Start")

	PORT_START("IN.2") // R7
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_R) PORT_NAME("Repeat-2")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // R8
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")

	PORT_START("IN.4") // R9
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")
INPUT_PORTS_END


static const UINT16 tandy12_output_pla[0x20] =
{
	// these are certain
	0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
	0x80, 0x00, 0x00, 0x00, 0x00,

	// rest is unused?
	0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static MACHINE_CONFIG_START( tandy12, tandy12_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", TMS1100, 400000) // RC osc. R=39K, C=47pf -> ~400kHz
	MCFG_TMS1XXX_OUTPUT_PLA(tandy12_output_pla)
	MCFG_TMS1XXX_READ_K_CB(READ8(tandy12_state, read_k))
	MCFG_TMS1XXX_WRITE_R_CB(WRITE16(tandy12_state, write_r))
	MCFG_TMS1XXX_WRITE_O_CB(WRITE16(tandy12_state, write_o))

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_tms1k_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_tandy12)

	/* no video! */

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( mathmagi )
	ROM_REGION( 0x800, "maincpu", 0 )
	ROM_LOAD( "mp1030", 0x0000, 0x800, CRC(a81d7ccb) SHA1(4756ce42f1ea28ce5fe6498312f8306f10370969) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_default_mpla.pla", 0, 867, BAD_DUMP CRC(62445fc9) SHA1(d6297f2a4bc7a870b76cc498d19dbb0ce7d69fec) ) // not verified
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_mathmagi_opla.pla", 0, 365, NO_DUMP )
ROM_END


ROM_START( amaztron )
	ROM_REGION( 0x0800, "maincpu", 0 )
	ROM_LOAD( "mp3405", 0x0000, 0x0800, CRC(9cbc0009) SHA1(17772681271b59280687492f37fa0859998f041d) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_amaztron_mpla.pla", 0, 867, CRC(03574895) SHA1(04407cabfb3adee2ee5e4218612cb06c12c540f4) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_amaztron_opla.pla", 0, 365, CRC(f3875384) SHA1(3c256a3db4f0aa9d93cf78124db39f4cbdc57e4a) )
ROM_END


ROM_START( tc4 )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mp7334", 0x0000, 0x1000, CRC(923f3821) SHA1(a9ae342d7ff8dae1dedcd1e4984bcfae68586581) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_default_mpla.pla", 0, 867, CRC(62445fc9) SHA1(d6297f2a4bc7a870b76cc498d19dbb0ce7d69fec) )
	ROM_REGION( 557, "maincpu:opla", 0 )
	ROM_LOAD( "tms1400_tc4_opla.pla", 0, 557, CRC(3b908725) SHA1(f83bf5faa5b3cb51f87adc1639b00d6f9a71ad19) )
ROM_END


ROM_START( ebball )
	ROM_REGION( 0x0400, "maincpu", 0 )
	ROM_LOAD( "mp0914", 0x0000, 0x0400, CRC(3c6fb05b) SHA1(b2fe4b3ca72d6b4c9bfa84d67f64afdc215e7178) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1000_ebball_mpla.pla", 0, 867, CRC(d33da3cf) SHA1(13c4ebbca227818db75e6db0d45b66ba5e207776) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1000_ebball_opla.pla", 0, 365, CRC(062bf5bb) SHA1(8d73ee35444299595961225528b153e3a5fe66bf) )
ROM_END


ROM_START( ebball2 )
	ROM_REGION( 0x0400, "maincpu", 0 )
	ROM_LOAD( "mp0923", 0x0000, 0x0400, CRC(077acfe2) SHA1(a294ce7614b2cdb01c754a7a50d60d807e3f0939) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1000_ebball2_mpla.pla", 0, 867, CRC(d33da3cf) SHA1(13c4ebbca227818db75e6db0d45b66ba5e207776) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1000_ebball2_opla.pla", 0, 365, CRC(adcd73d1) SHA1(d69e590d288ef99293d86716498f3971528e30de) )
ROM_END


ROM_START( ebball3 )
	ROM_REGION( 0x0800, "maincpu", 0 )
	ROM_LOAD( "mp1204", 0x0000, 0x0800, CRC(987a29ba) SHA1(9481ae244152187d85349d1a08e439e798182938) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_ebball3_mpla.pla", 0, 867, CRC(7cc90264) SHA1(c6e1cf1ffb178061da9e31858514f7cd94e86990) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_ebball3_opla.pla", 0, 365, CRC(00db663b) SHA1(6eae12503364cfb1f863df0e57970d3e766ec165) )
ROM_END


ROM_START( einvader )
	ROM_REGION( 0x0800, "maincpu", 0 )
	ROM_LOAD( "mp1211", 0x0000, 0x0800, CRC(b6efbe8e) SHA1(d7d54921dab22bb0c2956c896a5d5b56b6f64969) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_einvader_mpla.pla", 0, 867, CRC(7cc90264) SHA1(c6e1cf1ffb178061da9e31858514f7cd94e86990) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_einvader_opla.pla", 0, 365, CRC(490158e1) SHA1(61cace1eb09244663de98d8fb04d9459b19668fd) )
ROM_END


ROM_START( elecdet )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mp6100a", 0x0000, 0x1000, CRC(6f396bb8) SHA1(1f104d4ca9bee0d4572be4779b7551dfe20c4f04) )

	ROM_REGION( 1246, "maincpu:ipla", 0 )
	ROM_LOAD( "tms0980_default_ipla.pla", 0, 1246, CRC(42db9a38) SHA1(2d127d98028ec8ec6ea10c179c25e447b14ba4d0) )
	ROM_REGION( 1982, "maincpu:mpla", 0 )
	ROM_LOAD( "tms0980_default_mpla.pla", 0, 1982, CRC(3709014f) SHA1(d28ee59ded7f3b9dc3f0594a32a98391b6e9c961) )
	ROM_REGION( 352, "maincpu:opla", 0 )
	ROM_LOAD( "tms0980_elecdet_opla.pla", 0, 352, CRC(652d19c3) SHA1(75550c2b293453b6b9efed88c8cc77195a53161f) )
	ROM_REGION( 157, "maincpu:spla", 0 )
	ROM_LOAD( "tms0980_elecdet_spla.pla", 0, 157, CRC(399aa481) SHA1(72c56c58fde3fbb657d69647a9543b5f8fc74279) )
ROM_END


ROM_START( starwbc )
	ROM_REGION( 0x0800, "maincpu", 0 )
	ROM_LOAD( "mp3438a", 0x0000, 0x0800, CRC(c12b7069) SHA1(d1f39c69a543c128023ba11cc6228bacdfab04de) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_starwbc_mpla.pla", 0, 867, CRC(03574895) SHA1(04407cabfb3adee2ee5e4218612cb06c12c540f4) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_starwbc_opla.pla", 0, 365, CRC(d358a76d) SHA1(06b60b207540e9b726439141acadea9aba718013) )
ROM_END

ROM_START( starwbcp )
	ROM_REGION( 0x0800, "maincpu", 0 )
	ROM_LOAD( "us4270755", 0x0000, 0x0800, BAD_DUMP CRC(fb3332f2) SHA1(a79ac81e239983cd699b7cfcc55f89b203b2c9ec) ) // from patent US4270755, may have errors

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_starwbc_mpla.pla", 0, 867, CRC(03574895) SHA1(04407cabfb3adee2ee5e4218612cb06c12c540f4) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_starwbc_opla.pla", 0, 365, CRC(d358a76d) SHA1(06b60b207540e9b726439141acadea9aba718013) )
ROM_END


ROM_START( astro )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mp1133", 0x0000, 0x1000, CRC(bc21109c) SHA1(05a433cce587d5c0c2d28b5fda5f0853ea6726bf) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1400_astro_mpla.pla", 0, 867, CRC(7cc90264) SHA1(c6e1cf1ffb178061da9e31858514f7cd94e86990) )
	ROM_REGION( 557, "maincpu:opla", 0 )
	ROM_LOAD( "tms1400_astro_opla.pla", 0, 557, CRC(eb08957e) SHA1(62ae0d13a1eaafb34f1b27d7df51441b400ccd56) )
ROM_END


ROM_START( comp4 )
	ROM_REGION( 0x0400, "maincpu", 0 )
	ROM_LOAD( "tmc0904nl_cp0904a", 0x0000, 0x0400, CRC(6233ee1b) SHA1(738e109b38c97804b4ec52bed80b00a8634ad453) )

	ROM_REGION( 782, "maincpu:ipla", 0 )
	ROM_LOAD( "tms0970_default_ipla.pla", 0, 782, CRC(e038fc44) SHA1(dfc280f6d0a5828d1bb14fcd59ac29caf2c2d981) )
	ROM_REGION( 860, "maincpu:mpla", 0 )
	ROM_LOAD( "tms0970_comp4_mpla.pla", 0, 860, CRC(ee9d7d9e) SHA1(25484e18f6a07f7cdb21a07220e2f2a82fadfe7b) )
	ROM_REGION( 352, "maincpu:opla", 0 )
	ROM_LOAD( "tms0970_comp4_opla.pla", 0, 352, CRC(a0f887d1) SHA1(3c666663d484d5bed81e1014f8715aab8a3d489f) )
	ROM_REGION( 157, "maincpu:spla", 0 )
	ROM_LOAD( "tms0970_comp4_spla.pla", 0, 157, CRC(e5bddd90) SHA1(4b1c6512c70e5bcd23c2dbf0c88cd8aa2c632a10) )
ROM_END


ROM_START( simon )
	ROM_REGION( 0x0400, "maincpu", 0 )
	ROM_LOAD( "tms1000.u1", 0x0000, 0x0400, CRC(9961719d) SHA1(35dddb018a8a2b31f377ab49c1f0cb76951b81c0) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1000_simon_mpla.pla", 0, 867, CRC(52f7c1f1) SHA1(dbc2634dcb98eac173ad0209df487cad413d08a5) )
	ROM_REGION( 365, "maincpu:opla", 0 ) // unused
	ROM_LOAD( "tms1000_simon_opla.pla", 0, 365, CRC(2943c71b) SHA1(bd5bb55c57e7ba27e49c645937ec1d4e67506601) )
ROM_END


ROM_START( ssimon )
	ROM_REGION( 0x800, "maincpu", 0 )
	ROM_LOAD( "mp3476", 0x0000, 0x800, CRC(98200571) SHA1(cbd0bcfc11a534aa0be5d011584cdcac58ff437a) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_default_mpla.pla", 0, 867, CRC(62445fc9) SHA1(d6297f2a4bc7a870b76cc498d19dbb0ce7d69fec) )
	ROM_REGION( 365, "maincpu:opla", 0 ) // unused
	ROM_LOAD( "tms1100_ssimon_opla.pla", 0, 365, CRC(0fea09b0) SHA1(27a56fcf2b490e9a7dbbc6ad48cc8aaca4cada94) )
ROM_END


ROM_START( cnsector )
	ROM_REGION( 0x0400, "maincpu", 0 )
	ROM_LOAD( "mp0905bnl_za0379", 0x0000, 0x0400, CRC(201036e9) SHA1(b37fef86bb2bceaf0ac8bb3745b4702d17366914) )

	ROM_REGION( 782, "maincpu:ipla", 0 )
	ROM_LOAD( "tms0970_default_ipla.pla", 0, 782, CRC(e038fc44) SHA1(dfc280f6d0a5828d1bb14fcd59ac29caf2c2d981) )
	ROM_REGION( 860, "maincpu:mpla", 0 )
	ROM_LOAD( "tms0970_cnsector_mpla.pla", 0, 860, CRC(059f5bb4) SHA1(2653766f9fd74d41d44013bb6f54c0973a6080c9) )
	ROM_REGION( 352, "maincpu:opla", 0 )
	ROM_LOAD( "tms0970_cnsector_opla.pla", 0, 352, CRC(7c0bdcd6) SHA1(dade774097e8095dca5deac7b2367d0c701aca51) )
	ROM_REGION( 157, "maincpu:spla", 0 )
	ROM_LOAD( "tms0970_cnsector_spla.pla", 0, 157, CRC(56c37a4f) SHA1(18ecc20d2666e89673739056483aed5a261ae927) )
ROM_END


ROM_START( merlin )
	ROM_REGION( 0x800, "maincpu", 0 )
	ROM_LOAD( "mp3404", 0x0000, 0x800, CRC(7515a75d) SHA1(76ca3605d3fde1df62f79b9bb1f534c2a2ae0229) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_merlin_mpla.pla", 0, 867, CRC(03574895) SHA1(04407cabfb3adee2ee5e4218612cb06c12c540f4) )
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_merlin_opla.pla", 0, 365, CRC(3921b074) SHA1(12bd58e4d6676eb8c7059ef53598279e4f1a32ea) )
ROM_END


ROM_START( stopthie )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mp6101b", 0x0000, 0x1000, CRC(8bde5bb4) SHA1(8c318fcce67acc24c7ae361f575f28ec6f94665a) )

	ROM_REGION( 1246, "maincpu:ipla", 0 )
	ROM_LOAD( "tms0980_default_ipla.pla", 0, 1246, CRC(42db9a38) SHA1(2d127d98028ec8ec6ea10c179c25e447b14ba4d0) )
	ROM_REGION( 1982, "maincpu:mpla", 0 )
	ROM_LOAD( "tms0980_default_mpla.pla", 0, 1982, CRC(3709014f) SHA1(d28ee59ded7f3b9dc3f0594a32a98391b6e9c961) )
	ROM_REGION( 352, "maincpu:opla", 0 )
	ROM_LOAD( "tms0980_stopthie_opla.pla", 0, 352, CRC(50337a48) SHA1(4a9ea62ed797a9ac5190eec3bb6ebebb7814628c) )
	ROM_REGION( 157, "maincpu:spla", 0 )
	ROM_LOAD( "tms0980_stopthie_spla.pla", 0, 157, CRC(399aa481) SHA1(72c56c58fde3fbb657d69647a9543b5f8fc74279) )
ROM_END

ROM_START( stopthiep )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD16_WORD( "us4341385", 0x0000, 0x1000, CRC(07aec38a) SHA1(0a3d0956495c0d6d9ea771feae6c14a473a800dc) ) // from patent US4341385, data should be correct (it included checksums)

	ROM_REGION( 1246, "maincpu:ipla", 0 )
	ROM_LOAD( "tms0980_default_ipla.pla", 0, 1246, CRC(42db9a38) SHA1(2d127d98028ec8ec6ea10c179c25e447b14ba4d0) )
	ROM_REGION( 1982, "maincpu:mpla", 0 )
	ROM_LOAD( "tms0980_default_mpla.pla", 0, 1982, CRC(3709014f) SHA1(d28ee59ded7f3b9dc3f0594a32a98391b6e9c961) )
	ROM_REGION( 352, "maincpu:opla", 0 )
	ROM_LOAD( "tms0980_stopthie_opla.pla", 0, 352, CRC(50337a48) SHA1(4a9ea62ed797a9ac5190eec3bb6ebebb7814628c) )
	ROM_REGION( 157, "maincpu:spla", 0 )
	ROM_LOAD( "tms0980_stopthie_spla.pla", 0, 157, CRC(399aa481) SHA1(72c56c58fde3fbb657d69647a9543b5f8fc74279) )
ROM_END


ROM_START( bankshot )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mp7313", 0x0000, 0x1000, CRC(7a5016a9) SHA1(a8730dc8a282ffaa3d89e675f371d43eb39f39b4) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_default_mpla.pla", 0, 867, CRC(62445fc9) SHA1(d6297f2a4bc7a870b76cc498d19dbb0ce7d69fec) )
	ROM_REGION( 557, "maincpu:opla", 0 )
	ROM_LOAD( "tms1400_bankshot_opla.pla", 0, 557, CRC(7539283b) SHA1(f791fa98259fc10c393ff1961d4c93040f1a2932) )
ROM_END


ROM_START( splitsec )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mp7314", 0x0000, 0x1000, CRC(e94b2098) SHA1(f0fc1f56a829252185592a2508740354c50bedf8) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_default_mpla.pla", 0, 867, CRC(62445fc9) SHA1(d6297f2a4bc7a870b76cc498d19dbb0ce7d69fec) )
	ROM_REGION( 557, "maincpu:opla", 0 )
	ROM_LOAD( "tms1400_splitsec_opla.pla", 0, 557, CRC(7539283b) SHA1(f791fa98259fc10c393ff1961d4c93040f1a2932) )
ROM_END


ROM_START( tandy12 )
	ROM_REGION( 0x800, "maincpu", 0 )
	ROM_LOAD( "cd7282sl", 0x0000, 0x800, CRC(a10013dd) SHA1(42ebd3de3449f371b99937f9df39c240d15ac686) )

	ROM_REGION( 867, "maincpu:mpla", 0 )
	ROM_LOAD( "tms1100_default_mpla.pla", 0, 867, BAD_DUMP CRC(62445fc9) SHA1(d6297f2a4bc7a870b76cc498d19dbb0ce7d69fec) ) // not verified
	ROM_REGION( 365, "maincpu:opla", 0 )
	ROM_LOAD( "tms1100_tandy12_opla.pla", 0, 365, NO_DUMP )
ROM_END



/*    YEAR  NAME       PARENT COMPAT MACHINE   INPUT      INIT              COMPANY, FULLNAME, FLAGS */
COMP( 1980, mathmagi,  0,        0, mathmagi,  mathmagi,  driver_device, 0, "APF Electronics Inc.", "Mathemagician", GAME_SUPPORTS_SAVE | GAME_NO_SOUND_HW )

CONS( 1979, amaztron,  0,        0, amaztron,  amaztron,  driver_device, 0, "Coleco", "Amaze-A-Tron", GAME_SUPPORTS_SAVE )
CONS( 1981, tc4,       0,        0, tc4,       tc4,       driver_device, 0, "Coleco", "Total Control 4", GAME_SUPPORTS_SAVE )

CONS( 1979, ebball,    0,        0, ebball,    ebball,    driver_device, 0, "Entex", "Electronic Baseball (Entex)", GAME_SUPPORTS_SAVE )
CONS( 1979, ebball2,   0,        0, ebball2,   ebball2,   driver_device, 0, "Entex", "Electronic Baseball 2 (Entex)", GAME_SUPPORTS_SAVE )
CONS( 1980, ebball3,   0,        0, ebball3,   ebball3,   driver_device, 0, "Entex", "Electronic Baseball 3 (Entex)", GAME_SUPPORTS_SAVE )
CONS( 1980, einvader,  0,        0, einvader,  einvader,  driver_device, 0, "Entex", "Space Invader (Entex, TMS1100)", GAME_SUPPORTS_SAVE | GAME_REQUIRES_ARTWORK )

CONS( 1979, elecdet,   0,        0, elecdet,   elecdet,   driver_device, 0, "Ideal", "Electronic Detective", GAME_SUPPORTS_SAVE ) // ***

CONS( 1979, starwbc,   0,        0, starwbc,   starwbc,   driver_device, 0, "Kenner", "Star Wars - Electronic Battle Command", GAME_SUPPORTS_SAVE )
CONS( 1979, starwbcp,  starwbc,  0, starwbc,   starwbc,   driver_device, 0, "Kenner", "Star Wars - Electronic Battle Command (prototype)", GAME_SUPPORTS_SAVE )

COMP( 1979, astro,     0,        0, astro,     astro,     driver_device, 0, "Kosmos", "Astro", GAME_SUPPORTS_SAVE | GAME_NO_SOUND_HW )

CONS( 1977, comp4,     0,        0, comp4,     comp4,     driver_device, 0, "Milton Bradley", "Comp IV", GAME_SUPPORTS_SAVE | GAME_NO_SOUND_HW )
CONS( 1978, simon,     0,        0, simon,     simon,     driver_device, 0, "Milton Bradley", "Simon (Rev. A)", GAME_SUPPORTS_SAVE )
CONS( 1979, ssimon,    0,        0, ssimon,    ssimon,    driver_device, 0, "Milton Bradley", "Super Simon", GAME_SUPPORTS_SAVE )

CONS( 1977, cnsector,  0,        0, cnsector,  cnsector,  driver_device, 0, "Parker Brothers", "Code Name: Sector", GAME_SUPPORTS_SAVE | GAME_NO_SOUND_HW ) // ***
CONS( 1978, merlin,    0,        0, merlin,    merlin,    driver_device, 0, "Parker Brothers", "Merlin - The Electronic Wizard", GAME_SUPPORTS_SAVE )
CONS( 1979, stopthie,  0,        0, stopthief, stopthief, driver_device, 0, "Parker Brothers", "Stop Thief (Electronic Crime Scanner)", GAME_SUPPORTS_SAVE ) // ***
CONS( 1979, stopthiep, stopthie, 0, stopthief, stopthief, driver_device, 0, "Parker Brothers", "Stop Thief (Electronic Crime Scanner) (prototype)", GAME_SUPPORTS_SAVE | GAME_NOT_WORKING )
CONS( 1980, bankshot,  0,        0, bankshot,  bankshot,  driver_device, 0, "Parker Brothers", "Bank Shot - Electronic Pool", GAME_SUPPORTS_SAVE )
CONS( 1980, splitsec,  0,        0, splitsec,  splitsec,  driver_device, 0, "Parker Brothers", "Split Second", GAME_SUPPORTS_SAVE )

CONS( 1981, tandy12,   0,        0, tandy12,   tandy12,   driver_device, 0, "Tandy Radio Shack", "Tandy-12: Computerized Arcade", GAME_SUPPORTS_SAVE ) // some of the minigames: ***

// ***: As far as MESS is concerned, the game is emulated fine. But for it to be playable, it requires interaction
// with other, unemulatable, things eg. game board/pieces, playing cards, pen & paper, etc.
