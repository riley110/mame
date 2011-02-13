/***************************************************************************

        Digital Microsystems ZSBC-3

        11/01/2010 Skeleton driver.
        28/11/2010 Connected to a terminal
        11/01/2011 Converted to Modern.

****************************************************************************

        Monitor commands: [] indicates optional

        Bx = Boot from device x (0 to 7)
        Dx [y] = Dump memory (hex and ascii) in range x [to y]
        Fx y z = Fill memory x to y with z
        Gx = Execute program at address x
        Ix = Display IN of port x
        Ox y = Output y to port x
        Sx = Enter memory editing mode, press enter for next address
        Mx y = unknown (affects memory)
        Tx = unknown (does strange things)
        enter = dump memory from 9000 to 907F (why?)

****************************************************************************

        TODO:
        Everything really...

        Devices of all kinds;
        Use the other rom for something..

****************************************************************************/

#define ADDRESS_MAP_MODERN

#include "emu.h"
#include "cpu/z80/z80.h"
#include "machine/terminal.h"


class zsbc3_state : public driver_device
{
public:
	zsbc3_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config),
		  m_maincpu(*this, "maincpu"),
		  m_terminal(*this, "terminal")
	{ }

	required_device<cpu_device> m_maincpu;
	required_device<device_t> m_terminal;
	DECLARE_READ8_MEMBER(zsbc3_28_r);
	DECLARE_READ8_MEMBER(zsbc3_2a_r);
	DECLARE_WRITE8_MEMBER(zsbc3_28_w);
	DECLARE_WRITE8_MEMBER(zsbc3_kbd_put);
	UINT8 m_term_data;
};



WRITE8_MEMBER( zsbc3_state::zsbc3_28_w )
{
	terminal_write(m_terminal, 0, data);
}

READ8_MEMBER( zsbc3_state::zsbc3_28_r )
{
	UINT8 ret = m_term_data;
	m_term_data = 0;
	return ret;
}

READ8_MEMBER( zsbc3_state::zsbc3_2a_r )
{
	return 4 | ((m_term_data) ? 1 : 0);
}

static ADDRESS_MAP_START(zsbc3_mem, ADDRESS_SPACE_PROGRAM, 8, zsbc3_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE( 0x0000, 0x07ff ) AM_ROM
	AM_RANGE( 0x0800, 0xffff ) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( zsbc3_io , ADDRESS_SPACE_IO, 8, zsbc3_state)
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x28, 0x28) AM_READWRITE(zsbc3_28_r,zsbc3_28_w)
	AM_RANGE(0x2a, 0x2a) AM_READ(zsbc3_2a_r)
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( zsbc3 )
INPUT_PORTS_END


static MACHINE_RESET(zsbc3)
{
}

WRITE8_MEMBER( zsbc3_state::zsbc3_kbd_put )
{
	m_term_data = data;
}

static GENERIC_TERMINAL_INTERFACE( zsbc3_terminal_intf )
{
	DEVCB_DRIVER_MEMBER(zsbc3_state, zsbc3_kbd_put)
};


static MACHINE_CONFIG_START( zsbc3, zsbc3_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu",Z80, XTAL_16MHz /4)
	MCFG_CPU_PROGRAM_MAP(zsbc3_mem)
	MCFG_CPU_IO_MAP(zsbc3_io)

	MCFG_MACHINE_RESET(zsbc3)

	/* video hardware */
	MCFG_FRAGMENT_ADD( generic_terminal )

	MCFG_GENERIC_TERMINAL_ADD("terminal", zsbc3_terminal_intf)
MACHINE_CONFIG_END

/* ROM definition */
ROM_START( zsbc3 )
	ROM_REGION( 0x10000, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD( "54-3002_zsbc_monitor_1.09.bin", 0x0000, 0x0800, CRC(628588e9) SHA1(8f0d489147ec8382ca007236e0a95a83b6ebcd86))

	ROM_REGION( 0x10000, "hdc", ROMREGION_ERASEFF )
	ROM_LOAD( "54-8622_hdc13.bin", 0x0000, 0x0400, CRC(02c7cd6d) SHA1(494281ba081a0f7fbadfc30a7d2ea18c59e55101))
ROM_END

/* Driver */

/*    YEAR  NAME    PARENT  COMPAT   MACHINE    INPUT    INIT     COMPANY               FULLNAME       FLAGS */
COMP( 1980, zsbc3,  0,       0, 		zsbc3,	zsbc3,	 0, 	  "Digital Microsystems",   "ZSBC-3",		GAME_NOT_WORKING | GAME_NO_SOUND_HW)

