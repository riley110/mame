/*
    abc800.c

    MESS Driver by Curt Coder

    Luxor ABC 800C
    --------------
    (c) 1981 Luxor Datorer AB, Sweden

    CPU:            Z80 @ 3 MHz
    ROM:            32 KB
    RAM:            16 KB, 1 KB frame buffer, 16 KB high-resolution videoram (800C/HR)
    Resolution:     240x240
    Colors:         8

    Luxor ABC 800M
    --------------
    (c) 1981 Luxor Datorer AB, Sweden

    CPU:            Z80 @ 3 MHz
    ROM:            32 KB
    RAM:            16 KB, 2 KB frame buffer, 16 KB high-resolution videoram (800M/HR)
    CRTC:           6845
    Resolution:     480x240, 240x240 (HR)
    Colors:         2

    Luxor ABC 802
    -------------
    (c) 1983 Luxor Datorer AB, Sweden

    CPU:            Z80 @ 3 MHz
    ROM:            32 KB
    RAM:            16 KB, 2 KB frame buffer, 16 KB ram-floppy
    CRTC:           6845
    Resolution:     480x240
    Colors:         2

    Luxor ABC 806
    -------------
    (c) 1983 Luxor Datorer AB, Sweden

    CPU:            Z80 @ 3 MHz
    ROM:            32 KB
    RAM:            32 KB, 4 KB frame buffer, 128 KB scratch pad ram
    CRTC:           6845
    Resolution:     240x240, 256x240, 512x240
    Colors:         8

    http://www.devili.iki.fi/Computers/Luxor/
    http://hem.passagen.se/mani/abc/
*/

/*

    TODO:

    - ABC 77 keyboard
    - bit accurate Z80 SIO/2 (cassette)
    - floppy controller board
    - hard disks (ABC-850 10MB, ABC-852 20MB, ABC-856 60MB)

*/

/* Core includes */
#include "emu.h"
#include "includes/abc80x.h"

/* Components */
#include "cpu/z80/z80.h"
#include "cpu/z80/z80daisy.h"
#include "machine/serial.h"
#include "machine/z80ctc.h"
#include "machine/z80sio.h"
#include "machine/z80dart.h"
#include "machine/abcbus.h"
#include "machine/e0516.h"
#include "machine/abc77.h"
#include "machine/conkort.h"
#include "video/mc6845.h"
#include "sound/discrete.h"

/* Devices */
#include "devices/cassette.h"
#include "devices/printer.h"
#include "devices/messram.h"

static running_device *cassette_device_image(running_machine *machine)
{
	return machine->device(CASSETTE_TAG);
}

/* Discrete Sound */

static DISCRETE_SOUND_START( abc800 )
	DISCRETE_INPUT_LOGIC(NODE_01)
	DISCRETE_OUTPUT(NODE_01, 5000)
DISCRETE_SOUND_END

/* Keyboard HACK */

static int keylatch;

static const UINT8 abc800_keycodes[7*4][8] =
{
	// unshift
	{ 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38 },
	{ 0x39, 0x30, 0x2b, 0x60, 0x3c, 0x71, 0x77, 0x65 },
	{ 0x72, 0x74, 0x79, 0x75, 0x69, 0x6f, 0x70, 0x7d },
	{ 0x7e, 0x0d, 0x61, 0x73, 0x64, 0x66, 0x67, 0x68 },
	{ 0x6a, 0x6b, 0x6c, 0x7c, 0x7b, 0x27, 0x08, 0x7a },
	{ 0x78, 0x63, 0x76, 0x62, 0x6e, 0x6d, 0x2c, 0x2e },
	{ 0x2d, 0x09, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00 },

	// shift
	{ 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x2f, 0x28 },
	{ 0x29, 0x3d, 0x3f, 0x40, 0x3e, 0x51, 0x57, 0x45 },
	{ 0x52, 0x54, 0x59, 0x55, 0x49, 0x4f, 0x50, 0x5d },
	{ 0x5e, 0x0d, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48 },
	{ 0x4a, 0x4b, 0x4c, 0x5c, 0x5b, 0x2a, 0x08, 0x5a },
	{ 0x58, 0x43, 0x56, 0x42, 0x4e, 0x4d, 0x3b, 0x3a },
	{ 0x5f, 0x09, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00 },

	// control
	{ 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38 },
	{ 0x39, 0x30, 0x2b, 0x00, 0x7f, 0x11, 0x17, 0x05 },
	{ 0x12, 0x14, 0x19, 0x15, 0x09, 0x0f, 0x10, 0x1d },
	{ 0x1e, 0x0d, 0x01, 0x13, 0x04, 0x06, 0x07, 0x08 },
	{ 0x0a, 0x0b, 0x0c, 0x1c, 0x1b, 0x27, 0x08, 0x1a },
	{ 0x18, 0x03, 0x16, 0x02, 0x0e, 0x0d, 0x2c, 0x2e },
	{ 0x2d, 0x09, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00 },

	// control-shift
	{ 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x2f, 0x28 },
	{ 0x29, 0x3d, 0x3f, 0x00, 0x7f, 0x11, 0x17, 0x05 },
	{ 0x12, 0x14, 0x19, 0x15, 0x09, 0x1f, 0x00, 0x1d },
	{ 0x1e, 0x0d, 0x01, 0x13, 0x04, 0x06, 0x07, 0x08 },
	{ 0x0a, 0x1b, 0x1c, 0x1c, 0x1b, 0x2a, 0x08, 0x1a },
	{ 0x18, 0x03, 0x16, 0x02, 0x1e, 0x1d, 0x3b, 0x3a },
	{ 0x5f, 0x09, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

static void scan_keyboard(running_machine *machine)
{
	UINT8 keycode = 0;
	UINT8 data;
	int table = 0, row, col;
	static const char *const keynames[] = { "ROW0", "ROW1", "ROW2", "ROW3", "ROW4", "ROW5", "ROW6" };

	// shift, upper case
	if (input_port_read(machine, "ROW7") & 0x07)
	{
		table |= 0x01;
	}

	// ctrl
	if (input_port_read(machine, "ROW7") & 0x08)
	{
		table |= 0x02;
	}

	for (row = 0; row < 7; row++)
	{
		data = input_port_read(machine, keynames[row]);

		if (data != 0)
		{
			UINT8 ibit = 1;

			for (col = 0; col < 8; col++)
			{
				if (data & ibit) keycode = abc800_keycodes[row + (table * 7)][col];
				ibit <<= 1;
			}
		}
	}

	if (keycode)
	{
		if (keycode != keylatch)
		{
			z80dart_device *z80dart = machine->device<z80dart_device>(Z80DART_TAG);

			z80dart_dcdb_w(z80dart, 0);
			z80dart->receive_data(Z80DART_CH_B, keycode);

			keylatch = keycode;
		}
	}
	else
	{
		if (keylatch)
		{
			z80dart_device *z80dart = machine->device<z80dart_device>(Z80DART_TAG);

			z80dart_dcdb_w(z80dart, 1);
			z80dart->receive_data(Z80DART_CH_B, 0);

			keylatch = 0;
		}
	}
}

static TIMER_DEVICE_CALLBACK( keyboard_tick )
{
	scan_keyboard(timer.machine);
}

/* Memory Banking */

static void abc800_bankswitch(running_machine *machine)
{
	abc800_state *state = machine->driver_data<abc800_state>();
	address_space *program = cputag_get_address_space(machine, Z80_TAG, ADDRESS_SPACE_PROGRAM);

	if (state->fetch_charram)
	{
		/* HR video RAM selected */
		memory_install_ram(program, 0x0000, 0x3fff, 0, 0, state->videoram);
	}
	else
	{
		/* BASIC ROM selected */
		memory_install_rom(program, 0x0000, 0x3fff, 0, 0, memory_region(machine, Z80_TAG));
	}
}

static void abc802_bankswitch(running_machine *machine)
{
	abc802_state *state = machine->driver_data<abc802_state>();
	address_space *program = cputag_get_address_space(machine, Z80_TAG, ADDRESS_SPACE_PROGRAM);

	if (state->lrs)
	{
		/* ROM and video RAM selected */
		memory_install_readwrite_bank(program, 0x0000, 0x77ff, 0, 0, "bank1");
		memory_install_readwrite8_handler(program, 0x7800, 0x7fff, 0, 0, abc802_charram_r, abc802_charram_w);
	}
	else
	{
		/* low RAM selected */
		memory_install_readwrite_bank(program, 0x0000, 0x7fff, 0, 0, "bank1");
	}

	memory_set_bank(machine, "bank1", state->lrs);
}

static void abc806_bankswitch(running_machine *machine)
{
	abc806_state *state = machine->driver_data<abc806_state>();
	address_space *program = cputag_get_address_space(machine, Z80_TAG, ADDRESS_SPACE_PROGRAM);
	UINT32 videoram_mask = messram_get_size(machine->device("messram")) - (32 * 1024) - 1;
	int bank;
	char bank_name[10];

	if (!state->keydtr)
	{
		/* 32K block mapping */

		UINT32 videoram_start = (state->hrs & 0xf0) << 11;

		for (bank = 1; bank <= 8; bank++)
		{
			/* 0x0000-0x7FFF is video RAM */

			UINT16 start_addr = 0x1000 * (bank - 1);
			UINT16 end_addr = start_addr + 0xfff;
			UINT32 videoram_offset = (videoram_start + start_addr) & videoram_mask;
			sprintf(bank_name,"bank%d",bank);
			//logerror("%04x-%04x: Video RAM %04x (32K)\n", start_addr, end_addr, videoram_offset);

			memory_install_readwrite_bank(program, start_addr, end_addr, 0, 0, bank_name);
			memory_configure_bank(machine, bank_name, 1, 1, state->videoram + videoram_offset, 0);
			memory_set_bank(machine, bank_name, 1);
		}

		for (bank = 9; bank <= 16; bank++)
		{
			/* 0x8000-0xFFFF is main RAM */

			UINT16 start_addr = 0x1000 * (bank - 1);
			UINT16 end_addr = start_addr + 0xfff;
			sprintf(bank_name,"bank%d",bank);
			//logerror("%04x-%04x: Work RAM (32K)\n", start_addr, end_addr);

			memory_install_readwrite_bank(program, start_addr, end_addr, 0, 0, bank_name);
			memory_set_bank(machine, bank_name, 0);
		}
	}
	else
	{
		/* 4K block mapping */

		for (bank = 1; bank <= 16; bank++)
		{
			UINT16 start_addr = 0x1000 * (bank - 1);
			UINT16 end_addr = start_addr + 0xfff;
			UINT8 map = state->map[bank - 1];
			UINT32 videoram_offset = ((map & 0x7f) << 12) & videoram_mask;
			sprintf(bank_name,"bank%d",bank);
			if (BIT(map, 7) && state->eme)
			{
				/* map to video RAM */
				//logerror("%04x-%04x: Video RAM %04x (4K)\n", start_addr, end_addr, videoram_offset);

				memory_install_readwrite_bank(program, start_addr, end_addr, 0, 0, bank_name);
				memory_configure_bank(machine, bank_name, 1, 1, state->videoram + videoram_offset, 0);
				memory_set_bank(machine, bank_name, 1);
			}
			else
			{
				/* map to ROM/RAM */

				switch (bank)
				{
				case 1: case 2: case 3: case 4: case 5: case 6: case 7:
					/* ROM */
					//logerror("%04x-%04x: ROM (4K)\n", start_addr, end_addr);

					memory_install_read_bank(program, start_addr, end_addr, 0, 0, bank_name);
					memory_unmap_write(program, start_addr, end_addr, 0, 0);
					memory_set_bank(machine, bank_name, 0);
					break;

				case 8:
					/* ROM/char RAM */
					//logerror("%04x-%04x: ROM (4K)\n", start_addr, end_addr);

					memory_install_read_bank(program, 0x7000, 0x77ff, 0, 0, bank_name);
					memory_unmap_write(program, 0x7000, 0x77ff, 0, 0);
					memory_install_readwrite8_handler(program, 0x7800, 0x7fff, 0, 0, abc806_charram_r, abc806_charram_w);
					memory_set_bank(machine, bank_name, 0);
					break;

				default:
					/* work RAM */
					//logerror("%04x-%04x: Work RAM (4K)\n", start_addr, end_addr);

					memory_install_readwrite_bank(program, start_addr, end_addr, 0, 0, bank_name);
					memory_set_bank(machine, bank_name, 0);
					break;
				}
			}
		}
	}

	if (state->fetch_charram)
	{
		/* 30K block mapping */

		UINT32 videoram_start = (state->hrs & 0xf0) << 11;

		for (bank = 1; bank <= 8; bank++)
		{
			/* 0x0000-0x77FF is video RAM */

			UINT16 start_addr = 0x1000 * (bank - 1);
			UINT16 end_addr = start_addr + 0xfff;
			UINT32 videoram_offset = (videoram_start + start_addr) & videoram_mask;
			sprintf(bank_name,"bank%d",bank);
			//logerror("%04x-%04x: Video RAM %04x (30K)\n", start_addr, end_addr, videoram_offset);

			if (start_addr == 0x7000)
			{
				memory_install_readwrite_bank(program, 0x7000, 0x77ff, 0, 0, bank_name);
				memory_install_readwrite8_handler(program, 0x7800, 0x7fff, 0, 0, abc806_charram_r, abc806_charram_w);
			}
			else
			{
				memory_install_readwrite_bank(program, start_addr, end_addr, 0, 0, bank_name);
			}

			memory_configure_bank(machine, bank_name, 1, 1, state->videoram + videoram_offset, 0);
			memory_set_bank(machine, bank_name, 1);
		}
	}
}

static READ8_HANDLER( abc806_mai_r )
{
	abc806_state *state = space->machine->driver_data<abc806_state>();

	int bank = offset >> 12;

	return state->map[bank];
}

static WRITE8_HANDLER( abc806_mao_w )
{
	/*

        bit     description

        0       physical block address bit 0
        1       physical block address bit 1
        2       physical block address bit 2
        3       physical block address bit 3
        4       physical block address bit 4
        5
        6
        7       allocate block

    */

	abc806_state *state = space->machine->driver_data<abc806_state>();

	int bank = offset >> 12;

	state->map[bank] = data;

	abc806_bankswitch(space->machine);
}

/* Pling */

static READ8_HANDLER( abc800_pling_r )
{
	running_device *discrete = space->machine->device("discrete");
	abc800_state *state = space->machine->driver_data<abc800_state>();

	state->pling = !state->pling;

	discrete_sound_w(discrete, NODE_01, state->pling);

	return 0xff;
}

static READ8_HANDLER( abc802_pling_r )
{
	running_device *discrete = space->machine->device("discrete");
	abc802_state *state = space->machine->driver_data<abc802_state>();

	state->pling = !state->pling;

	discrete_sound_w(discrete, NODE_01, state->pling);

	return 0xff;
}

/* Memory Maps */

// ABC 800M

static ADDRESS_MAP_START( abc800m_map, ADDRESS_SPACE_PROGRAM, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x3fff) AM_RAM AM_BASE_MEMBER(abc800_state, videoram)
	AM_RANGE(0x4000, 0x77ff) AM_ROM
	AM_RANGE(0x7800, 0x7fff) AM_RAM AM_BASE_MEMBER(abc800_state, charram)
	AM_RANGE(0x8000, 0xffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( abc800m_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x00) AM_MIRROR(0x18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_inp_r, abcbus_utp_w)
	AM_RANGE(0x01, 0x01) AM_MIRROR(0x18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_stat_r, abcbus_cs_w)
	AM_RANGE(0x02, 0x02) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c1_w)
	AM_RANGE(0x03, 0x03) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c2_w)
	AM_RANGE(0x04, 0x04) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c3_w)
	AM_RANGE(0x05, 0x05) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c4_w)
	AM_RANGE(0x05, 0x05) AM_MIRROR(0x18) AM_READ(abc800_pling_r)
	AM_RANGE(0x06, 0x06) AM_MIRROR(0x18) AM_WRITE(abc800_hrs_w)
	AM_RANGE(0x07, 0x07) AM_MIRROR(0x18) AM_DEVREAD(ABCBUS_TAG, abcbus_rst_r) AM_WRITE(abc800_hrc_w)
	AM_RANGE(0x20, 0x23) AM_MIRROR(0x0c) AM_DEVREADWRITE(Z80DART_TAG, z80dart_ba_cd_r, z80dart_ba_cd_w)
	AM_RANGE(0x31, 0x31) AM_MIRROR(0x06) AM_DEVREAD(MC6845_TAG, mc6845_register_r)
	AM_RANGE(0x38, 0x38) AM_MIRROR(0x06) AM_DEVWRITE(MC6845_TAG, mc6845_address_w)
	AM_RANGE(0x39, 0x39) AM_MIRROR(0x06) AM_DEVWRITE(MC6845_TAG, mc6845_register_w)
	AM_RANGE(0x40, 0x43) AM_MIRROR(0x1c) AM_DEVREADWRITE(Z80SIO_TAG, z80sio_ba_cd_r, z80sio_ba_cd_w)
	AM_RANGE(0x60, 0x63) AM_MIRROR(0x1c) AM_DEVREADWRITE(Z80CTC_TAG, z80ctc_r, z80ctc_w)
ADDRESS_MAP_END

// ABC 800C

static ADDRESS_MAP_START( abc800c_map, ADDRESS_SPACE_PROGRAM, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x3fff) AM_RAM AM_BASE_MEMBER(abc800_state, videoram)
	AM_RANGE(0x4000, 0x7bff) AM_ROM
	AM_RANGE(0x7c00, 0x7fff) AM_RAM AM_BASE_MEMBER(abc800_state, charram)
	AM_RANGE(0x8000, 0xffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( abc800c_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x00) AM_MIRROR(0x18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_inp_r, abcbus_utp_w)
	AM_RANGE(0x01, 0x01) AM_MIRROR(0x18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_stat_r, abcbus_cs_w)
	AM_RANGE(0x02, 0x02) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c1_w)
	AM_RANGE(0x03, 0x03) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c2_w)
	AM_RANGE(0x04, 0x04) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c3_w)
	AM_RANGE(0x05, 0x05) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c4_w)
	AM_RANGE(0x05, 0x05) AM_MIRROR(0x18) AM_READ(abc800_pling_r)
	AM_RANGE(0x06, 0x06) AM_MIRROR(0x18) AM_WRITE(abc800_hrs_w)
	AM_RANGE(0x07, 0x07) AM_MIRROR(0x18) AM_DEVREAD(ABCBUS_TAG, abcbus_rst_r) AM_WRITE(abc800_hrc_w)
	AM_RANGE(0x20, 0x23) AM_MIRROR(0x0c) AM_DEVREADWRITE(Z80DART_TAG, z80dart_ba_cd_r, z80dart_ba_cd_w)
	AM_RANGE(0x40, 0x43) AM_MIRROR(0x1c) AM_DEVREADWRITE(Z80SIO_TAG, z80sio_ba_cd_r, z80sio_ba_cd_w)
	AM_RANGE(0x60, 0x63) AM_MIRROR(0x1c) AM_DEVREADWRITE(Z80CTC_TAG, z80ctc_r, z80ctc_w)
ADDRESS_MAP_END

// ABC 802

static ADDRESS_MAP_START( abc802_map, ADDRESS_SPACE_PROGRAM, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x7fff) AM_RAMBANK("bank1")
	AM_RANGE(0x8000, 0xffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( abc802_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x00) AM_MIRROR(0x18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_inp_r, abcbus_utp_w)
	AM_RANGE(0x01, 0x01) AM_MIRROR(0x18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_stat_r, abcbus_cs_w)
	AM_RANGE(0x02, 0x02) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c1_w)
	AM_RANGE(0x03, 0x03) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c2_w)
	AM_RANGE(0x04, 0x04) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c3_w)
	AM_RANGE(0x05, 0x05) AM_MIRROR(0x18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c4_w)
	AM_RANGE(0x05, 0x05) AM_MIRROR(0x08) AM_READ(abc802_pling_r)
	AM_RANGE(0x07, 0x07) AM_MIRROR(0x18) AM_DEVREAD(ABCBUS_TAG, abcbus_rst_r)
	AM_RANGE(0x20, 0x23) AM_MIRROR(0x0c) AM_DEVREADWRITE(Z80DART_TAG, z80dart_ba_cd_r, z80dart_ba_cd_w)
	AM_RANGE(0x31, 0x31) AM_MIRROR(0x06) AM_DEVREAD(MC6845_TAG, mc6845_register_r)
	AM_RANGE(0x38, 0x38) AM_MIRROR(0x06) AM_DEVWRITE(MC6845_TAG, mc6845_address_w)
	AM_RANGE(0x39, 0x39) AM_MIRROR(0x06) AM_DEVWRITE(MC6845_TAG, mc6845_register_w)
	AM_RANGE(0x40, 0x43) AM_MIRROR(0x1c) AM_DEVREADWRITE(Z80SIO_TAG, z80sio_ba_cd_r, z80sio_ba_cd_w)
	AM_RANGE(0x60, 0x63) AM_MIRROR(0x1c) AM_DEVREADWRITE(Z80CTC_TAG, z80ctc_r, z80ctc_w)
ADDRESS_MAP_END

// ABC 806

static ADDRESS_MAP_START( abc806_map, ADDRESS_SPACE_PROGRAM, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x0fff) AM_RAMBANK("bank1")
	AM_RANGE(0x1000, 0x1fff) AM_RAMBANK("bank2")
	AM_RANGE(0x2000, 0x2fff) AM_RAMBANK("bank3")
	AM_RANGE(0x3000, 0x3fff) AM_RAMBANK("bank4")
	AM_RANGE(0x4000, 0x4fff) AM_RAMBANK("bank5")
	AM_RANGE(0x5000, 0x5fff) AM_RAMBANK("bank6")
	AM_RANGE(0x6000, 0x6fff) AM_RAMBANK("bank7")
	AM_RANGE(0x7000, 0x7fff) AM_RAMBANK("bank8")
	AM_RANGE(0x8000, 0x8fff) AM_RAMBANK("bank9")
	AM_RANGE(0x9000, 0x9fff) AM_RAMBANK("bank10")
	AM_RANGE(0xa000, 0xafff) AM_RAMBANK("bank11")
	AM_RANGE(0xb000, 0xbfff) AM_RAMBANK("bank12")
	AM_RANGE(0xc000, 0xcfff) AM_RAMBANK("bank13")
	AM_RANGE(0xd000, 0xdfff) AM_RAMBANK("bank14")
	AM_RANGE(0xe000, 0xefff) AM_RAMBANK("bank15")
	AM_RANGE(0xf000, 0xffff) AM_RAMBANK("bank16")
ADDRESS_MAP_END

static ADDRESS_MAP_START( abc806_io_map, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00, 0x00) AM_MIRROR(0xff18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_inp_r, abcbus_utp_w)
	AM_RANGE(0x01, 0x01) AM_MIRROR(0xff18) AM_DEVREADWRITE(ABCBUS_TAG, abcbus_stat_r, abcbus_cs_w)
	AM_RANGE(0x02, 0x02) AM_MIRROR(0xff18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c1_w)
	AM_RANGE(0x03, 0x03) AM_MIRROR(0xff18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c2_w)
	AM_RANGE(0x04, 0x04) AM_MIRROR(0xff18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c3_w)
	AM_RANGE(0x05, 0x05) AM_MIRROR(0xff18) AM_DEVWRITE(ABCBUS_TAG, abcbus_c4_w)
	AM_RANGE(0x06, 0x06) AM_MIRROR(0xff18) AM_WRITE(abc806_hrs_w)
	AM_RANGE(0x07, 0x07) AM_MIRROR(0xff18) AM_DEVREAD(ABCBUS_TAG, abcbus_rst_r) AM_WRITE(abc806_hrc_w)
	AM_RANGE(0x20, 0x23) AM_MIRROR(0xff0c) AM_DEVREADWRITE(Z80DART_TAG, z80dart_ba_cd_r, z80dart_ba_cd_w)
	AM_RANGE(0x31, 0x31) AM_MIRROR(0xff06) AM_DEVREAD(MC6845_TAG, mc6845_register_r)
	AM_RANGE(0x34, 0x34) AM_MIRROR(0xff00) AM_MASK(0xff00) AM_READWRITE(abc806_mai_r, abc806_mao_w)
	AM_RANGE(0x35, 0x35) AM_MIRROR(0xff00) AM_READWRITE(abc806_ami_r, abc806_amo_w)
	AM_RANGE(0x36, 0x36) AM_MIRROR(0xff00) AM_READWRITE(abc806_sti_r, abc806_sto_w)
	AM_RANGE(0x37, 0x37) AM_MIRROR(0xff00) AM_MASK(0xff00) AM_READWRITE(abc806_cli_r, abc806_sso_w)
	AM_RANGE(0x38, 0x38) AM_MIRROR(0xff06) AM_DEVWRITE(MC6845_TAG, mc6845_address_w)
	AM_RANGE(0x39, 0x39) AM_MIRROR(0xff06) AM_DEVWRITE(MC6845_TAG, mc6845_register_w)
	AM_RANGE(0x40, 0x43) AM_MIRROR(0xff1c) AM_DEVREADWRITE(Z80SIO_TAG, z80sio_ba_cd_r, z80sio_ba_cd_w)
	AM_RANGE(0x60, 0x63) AM_MIRROR(0xff1c) AM_DEVREADWRITE(Z80CTC_TAG, z80ctc_r, z80ctc_w)
ADDRESS_MAP_END

/* Input Ports */

static INPUT_PORTS_START( fake_keyboard )
	PORT_START("ROW0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_1) PORT_CHAR('1') PORT_CHAR('!')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_2) PORT_CHAR('2') PORT_CHAR('"')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_3) PORT_CHAR('3') PORT_CHAR('#')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("4 \xC2\xA4") PORT_CODE(KEYCODE_4) PORT_CHAR('4') PORT_CHAR(0x00A4)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_5) PORT_CHAR('5') PORT_CHAR('%')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_6) PORT_CHAR('6') PORT_CHAR('&')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_7) PORT_CHAR('7') PORT_CHAR('/')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_8) PORT_CHAR('8') PORT_CHAR('(')

	PORT_START("ROW1")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_9) PORT_CHAR('9') PORT_CHAR(')')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_0) PORT_CHAR('0') PORT_CHAR('=')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_MINUS) PORT_CHAR('+') PORT_CHAR('?')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_EQUALS) PORT_CHAR(0x00E9) PORT_CHAR(0x00C9)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_BACKSLASH) PORT_CHAR('<') PORT_CHAR('>')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Q) PORT_CHAR('q') PORT_CHAR('Q')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_W) PORT_CHAR('w') PORT_CHAR('W')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_E) PORT_CHAR('e') PORT_CHAR('E')

	PORT_START("ROW2")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_R) PORT_CHAR('r') PORT_CHAR('R')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_T) PORT_CHAR('t') PORT_CHAR('T')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Y) PORT_CHAR('y') PORT_CHAR('Y')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_U) PORT_CHAR('u') PORT_CHAR('U')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_I) PORT_CHAR('i') PORT_CHAR('I')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_O) PORT_CHAR('o') PORT_CHAR('O')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_P) PORT_CHAR('p') PORT_CHAR('P')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR(0x00E5) PORT_CHAR(0x00C5)

	PORT_START("ROW3")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(0x00FC) PORT_CHAR(0x00DC)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("RETURN") PORT_CODE(KEYCODE_ENTER) PORT_CHAR('\r')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_A) PORT_CHAR('a') PORT_CHAR('A')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_S) PORT_CHAR('s') PORT_CHAR('S')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_D) PORT_CHAR('d') PORT_CHAR('D')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F) PORT_CHAR('f') PORT_CHAR('F')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_G) PORT_CHAR('g') PORT_CHAR('G')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_H) PORT_CHAR('h') PORT_CHAR('H')

	PORT_START("ROW4")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_J) PORT_CHAR('j') PORT_CHAR('J')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_K) PORT_CHAR('k') PORT_CHAR('K')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_L) PORT_CHAR('l') PORT_CHAR('L')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_COLON) PORT_CHAR(0x00F6) PORT_CHAR(0x00D6)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_QUOTE) PORT_CHAR(0x00E4) PORT_CHAR(0x00C4)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_BACKSLASH) PORT_CHAR('\'') PORT_CHAR('*')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xE2\x86\x90") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(8)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Z) PORT_CHAR('z') PORT_CHAR('Z')

	PORT_START("ROW5")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_X) PORT_CHAR('x') PORT_CHAR('X')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_C) PORT_CHAR('c') PORT_CHAR('C')
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_V) PORT_CHAR('v') PORT_CHAR('V')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_B) PORT_CHAR('b') PORT_CHAR('B')
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_N) PORT_CHAR('n') PORT_CHAR('N')
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_M) PORT_CHAR('m') PORT_CHAR('M')
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_COMMA) PORT_CHAR(',') PORT_CHAR(';')
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_STOP) PORT_CHAR('.') PORT_CHAR(':')

	PORT_START("ROW6")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_SLASH) PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("\xE2\x86\x92") PORT_CODE(KEYCODE_RIGHT) PORT_CHAR(UCHAR_MAMEKEY(RIGHT))
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("SPACE") PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ROW7")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("LEFT SHIFT") PORT_CODE(KEYCODE_LSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("RIGHT SHIFT") PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("UPPER CASE") PORT_CODE(KEYCODE_CAPSLOCK) PORT_CHAR(UCHAR_MAMEKEY(CAPSLOCK)) PORT_TOGGLE
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("CTRL") PORT_CODE(KEYCODE_LCONTROL) PORT_CHAR(UCHAR_MAMEKEY(LCONTROL))
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END

static INPUT_PORTS_START( abc800 )
//  PORT_INCLUDE(abc77)
	PORT_INCLUDE(fake_keyboard)

	PORT_START("SB")
	PORT_DIPNAME( 0xff, 0xaa, "Serial Communications" ) PORT_DIPLOCATION("SB:1,2,3,4,5,6,7,8")
	PORT_DIPSETTING(    0xaa, "Asynchronous, Single Speed" )
	PORT_DIPSETTING(    0x2e, "Asynchronous, Split Speed" )
	PORT_DIPSETTING(    0x50, "Synchronous" )
	PORT_DIPSETTING(    0x8b, "ABC NET" )

	PORT_START("FLOPPY")
	PORT_CONFNAME( 0x07, 0x00, "Floppy Drive" )
	PORT_CONFSETTING(    0x00, "ABC 830 (160KB)" )
	PORT_CONFSETTING(    0x01, "ABC 832/834 (640KB)" )
	PORT_CONFSETTING(    0x02, "ABC 838 (1MB)" )
	PORT_CONFSETTING(    0x03, "ABC 850 (640KB/HDD 10MB)" )
	PORT_CONFSETTING(    0x04, "ABC 852 (640KB/HDD 20MB)" )
	PORT_CONFSETTING(    0x05, "ABC 856 (640KB/HDD 64MB)" )

	PORT_INCLUDE(luxor_55_21046)
INPUT_PORTS_END

static INPUT_PORTS_START( abc802 )
	PORT_INCLUDE(abc800)

	PORT_START("CONFIG")
	PORT_CONFNAME( 0x01, 0x00, "Clear Screen Time Out" )
	PORT_CONFSETTING(    0x00, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x01, DEF_STR( On ) )
	PORT_CONFNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_CONFSETTING(    0x00, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x02, DEF_STR( On ) )
	PORT_CONFNAME( 0x04, 0x00, "Characters Per Line" )
	PORT_CONFSETTING(    0x00, "40" )
	PORT_CONFSETTING(    0x04, "80" )
	PORT_CONFNAME( 0x08, 0x08, "Frame Frequency" )
	PORT_CONFSETTING(    0x00, "60 Hz" )
	PORT_CONFSETTING(    0x08, "50 Hz" )
INPUT_PORTS_END

static INPUT_PORTS_START( abc806 )
	PORT_INCLUDE(abc800)
INPUT_PORTS_END

/* ABC 77 */

static ABC77_INTERFACE( abc800_abc77_intf )
{
	DEVCB_NULL,
	DEVCB_DEVICE_LINE(Z80DART_TAG, z80dart_rxtxcb_w),
	DEVCB_DEVICE_LINE(Z80DART_TAG, z80dart_dcdb_w)
};

/* Z80 CTC */

static TIMER_DEVICE_CALLBACK( ctc_tick )
{
	running_device *z80ctc = timer.machine->device(Z80CTC_TAG);

	z80ctc_trg0_w(z80ctc, 1);
	z80ctc_trg0_w(z80ctc, 0);

	z80ctc_trg1_w(z80ctc, 1);
	z80ctc_trg1_w(z80ctc, 0);

	z80ctc_trg2_w(z80ctc, 1);
	z80ctc_trg2_w(z80ctc, 0);
}

static WRITE_LINE_DEVICE_HANDLER( ctc_z0_w )
{
	//running_device *z80sio = device->machine->device(Z80SIO_TAG);

	UINT8 sb = input_port_read(device->machine, "SB");

	if (BIT(sb, 2))
	{
		/* connected to SIO/2 TxCA, CTC CLK/TRG3 */
		//z80sio_txca_w(z80sio, state);
		z80ctc_trg3_w(device, state);
	}

	/* connected to SIO/2 RxCB through a thingy */
	//z80sio_rxcb = ?
	//z80sio_rxcb_w(z80sio, z80sio_rxcb);

	/* connected to SIO/2 TxCB through a JK divide by 2 */
	//z80sio_txcb = !z80sio_txcb;
	//z80sio_txcb_w(z80sio, z80sio_txcb);
}

static WRITE_LINE_DEVICE_HANDLER( ctc_z1_w )
{
	//running_device *z80sio = device->machine->device(Z80SIO_TAG);

	UINT8 sb = input_port_read(device->machine, "SB");

	if (BIT(sb, 3))
	{
		/* connected to SIO/2 RxCA */
		//z80sio_rxca_w(z80sio, state);
	}

	if (BIT(sb, 4))
	{
		/* connected to SIO/2 TxCA, CTC CLK/TRG3 */
		//z80sio_txca_w(z80sio, state);
		z80ctc_trg3_w(device, state);
	}
}

static WRITE_LINE_DEVICE_HANDLER( ctc_z2_w )
{
	running_device *z80dart = device->machine->device(Z80DART_TAG);

	/* connected to DART channel A clock inputs */
	z80dart_rxca_w(z80dart, state);
	z80dart_txca_w(z80dart, state);
}

static Z80CTC_INTERFACE( ctc_intf )
{
	0,              				/* timer disables */
	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0),	/* interrupt handler */
	DEVCB_LINE(ctc_z0_w),			/* ZC/TO0 callback */
	DEVCB_LINE(ctc_z1_w),			/* ZC/TO1 callback */
	DEVCB_LINE(ctc_z2_w)    		/* ZC/TO2 callback */
};

/* Z80 SIO/2 */

static void sio_interrupt(running_device *device, int state)
{
	cputag_set_input_line(device->machine, Z80_TAG, INPUT_LINE_IRQ0, state);
}

static WRITE8_DEVICE_HANDLER( sio_dtr_w )
{
	if (offset == 1)
	{
		/* cassette motor control */
		cassette_change_state(cassette_device_image(device->machine), data ? CASSETTE_MOTOR_ENABLED : CASSETTE_MOTOR_DISABLED, CASSETTE_MASK_MOTOR);
	}
}

static WRITE8_DEVICE_HANDLER( sio_serial_transmit )
{
	if (offset == 1)
	{
		/* cassette dfd out */
	}
}

static int sio_serial_receive( running_device *device, int channel )
{
	if (channel == 1)
	{
		/* cassette dfd in */
	}

	return -1;
}

static const z80sio_interface sio_intf =
{
	sio_interrupt,			/* interrupt handler */
	sio_dtr_w,				/* DTR changed handler */
	0,						/* RTS changed handler */
	0,						/* BREAK changed handler */
	sio_serial_transmit,	/* transmit handler */
	sio_serial_receive		/* receive handler */
};

/* Z80 DART */

static Z80DART_INTERFACE( abc800_dart_intf )
{
	0,
	0,
	0,

	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,

	DEVCB_NULL, /* DEVCB_DEVICE_LINE(ABC77_TAG, abc77_txd_r), */
	DEVCB_NULL, /* DEVCB_DEVICE_LINE(ABC77_TAG, abc77_rxd_w), */
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,

	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0)
};

static WRITE_LINE_DEVICE_HANDLER( abc802_dart_dtrb_r )
{
	abc802_state *driver_state = device->machine->driver_data<abc802_state>();

	/* low RAM select */
	driver_state->lrs = state;

	abc802_bankswitch(device->machine);
}

static WRITE_LINE_DEVICE_HANDLER( abc802_dart_rtsb_r )
{
	abc802_state *driver_state = device->machine->driver_data<abc802_state>();

	/* _MUX 80/40 */
	driver_state->mux80_40 = state;
}

static Z80DART_INTERFACE( abc802_dart_intf )
{
	0,
	0,
	0,

	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,

	DEVCB_NULL, /* DEVCB_DEVICE_LINE(ABC77_TAG, abc77_txd_r), */
	DEVCB_NULL, /* DEVCB_DEVICE_LINE(ABC77_TAG, abc77_rxd_w), */
	DEVCB_LINE(abc802_dart_dtrb_r),
	DEVCB_LINE(abc802_dart_rtsb_r),
	DEVCB_NULL,

	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0)
};

static WRITE_LINE_DEVICE_HANDLER( abc806_dart_dtrb_w )
{
	abc806_state *driver_state = device->machine->driver_data<abc806_state>();

	driver_state->keydtr = state;

	abc806_bankswitch(device->machine);
}

static Z80DART_INTERFACE( abc806_dart_intf )
{
	0,
	0,
	0,

	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,

	DEVCB_NULL, /* DEVCB_DEVICE_LINE(ABC77_TAG, abc77_txd_r), */
	DEVCB_NULL, /* DEVCB_DEVICE_LINE(ABC77_TAG, abc77_rxd_w), */
	DEVCB_LINE(abc806_dart_dtrb_w),
	DEVCB_NULL,
	DEVCB_NULL,

	DEVCB_CPU_INPUT_LINE(Z80_TAG, INPUT_LINE_IRQ0)
};

/* Z80 Daisy Chain */

static const z80_daisy_config abc800_daisy_chain[] =
{
	{ Z80CTC_TAG },
	{ Z80SIO_TAG },
	{ Z80DART_TAG },
	{ NULL }
};

/* ABC BUS */

static ABCBUS_DAISY( abcbus_daisy )
{
	{ LUXOR_55_21046_ABCBUS("abc830") },
	{ NULL }
};

static ABCBUS_DAISY( abc802_abcbus_daisy )
{
//  { LUXOR_55_21046_ABCBUS("abc830") }, won't boot with this enabled
	{ NULL }
};

/* Machine Initialization */

static MACHINE_START( abc800 )
{
	abc800_state *state = machine->driver_data<abc800_state>();

	/* find devices */
	state->z80ctc = machine->device(Z80CTC_TAG);
	state->z80dart = machine->device(Z80DART_TAG);
	state->z80sio = machine->device(Z80SIO_TAG);
	state->abc77 = machine->device(ABC77_TAG);
	state->cassette = machine->device(CASSETTE_TAG);

	/* register for state saving */
	state_save_register_global(machine, state->fetch_charram);
	state_save_register_global(machine, state->pling);
}

static MACHINE_RESET( abc800 )
{
	abc800_state *state = machine->driver_data<abc800_state>();

	state->fetch_charram = 0;
	abc800_bankswitch(machine);
}

static MACHINE_START( abc802 )
{
	abc802_state *state = machine->driver_data<abc802_state>();

	/* find devices */
	state->z80ctc = machine->device(Z80CTC_TAG);
	state->z80dart = machine->device(Z80DART_TAG);
	state->z80sio = machine->device(Z80SIO_TAG);
	state->abc77 = machine->device(ABC77_TAG);
	state->cassette = machine->device(CASSETTE_TAG);

	/* configure memory */
	memory_configure_bank(machine, "bank1", 0, 1, messram_get_ptr(machine->device("messram")), 0);
	memory_configure_bank(machine, "bank1", 1, 1, memory_region(machine, Z80_TAG), 0);

	/* register for state saving */
	state_save_register_global(machine, state->lrs);
	state_save_register_global(machine, state->pling);
}

static MACHINE_RESET( abc802 )
{
	abc802_state *state = machine->driver_data<abc802_state>();

	UINT8 config = input_port_read(machine, "CONFIG");

	/* memory banking */
	memory_set_bank(machine, "bank1", 1);

	/* clear screen time out (S1) */
	z80sio_set_dcd(state->z80sio, 1, BIT(config, 0));

	/* unknown (S2) */
	z80sio_set_cts(state->z80sio, 1, BIT(config, 1));

	/* 40/80 char (S3) */
	z80dart_ria_w(state->z80dart, BIT(config, 2)); // 0 = 40, 1 = 80

	/* 50/60 Hz */
	z80dart_ctsb_w(state->z80dart, BIT(config, 3)); // 0 = 50Hz, 1 = 60Hz
}

static MACHINE_START( abc806 )
{
	abc806_state *state = machine->driver_data<abc806_state>();

	UINT8 *mem = memory_region(machine, Z80_TAG);
	UINT32 videoram_size = messram_get_size(machine->device("messram")) - (32 * 1024);
	int bank;
	char bank_name[10];

	/* find devices */
	state->z80ctc = machine->device(Z80CTC_TAG);
	state->z80dart = machine->device(Z80DART_TAG);
	state->z80sio = machine->device(Z80SIO_TAG);
	state->e0516 = machine->device(E0516_TAG);
	state->abc77 = machine->device(ABC77_TAG);
	state->cassette = machine->device(CASSETTE_TAG);

	/* setup memory banking */
	state->videoram = auto_alloc_array(machine, UINT8, videoram_size);

	for (bank = 1; bank <= 16; bank++)
	{
		sprintf(bank_name,"bank%d",bank);
		memory_configure_bank(machine, bank_name, 0, 1, mem + (0x1000 * (bank - 1)), 0);
		memory_configure_bank(machine, bank_name, 1, 1, state->videoram, 0);
		memory_set_bank(machine, bank_name, 0);
	}

	/* register for state saving */
	state_save_register_global(machine, state->keydtr);
	state_save_register_global(machine, state->eme);
	state_save_register_global(machine, state->fetch_charram);
	state_save_register_global_array(machine, state->map);
}

static MACHINE_RESET( abc806 )
{
	abc806_state *state = machine->driver_data<abc806_state>();

	/* setup memory banking */
	int bank;
	char bank_name[10];

	for (bank = 1; bank <= 16; bank++)
	{
		sprintf(bank_name,"bank%d",bank);
		memory_set_bank(machine, bank_name, 0);
	}

	abc806_bankswitch(machine);

	/* clear STO lines */
	state->eme = 0;
	state->_40 = 0;
	state->hru2_a8 = 0;
	state->txoff = 0;
	e0516_cs_w(state->e0516, 1);
	e0516_clk_w(state->e0516, 1);
	e0516_dio_w(state->e0516, 1);

	/* 40/80 char */
	z80dart_ria_w(state->z80dart, 1); // 0 = 40, 1 = 80

	/* 50/60 Hz */
	z80dart_ctsb_w(state->z80dart, 0); // 0 = 50Hz, 1 = 60Hz
}

/* Machine Drivers */

static MACHINE_DRIVER_START( abc800m )
	MDRV_DRIVER_DATA(abc800_state)

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80_TAG, Z80, ABC800_X01/2/2)
	MDRV_CPU_CONFIG(abc800_daisy_chain)
	MDRV_CPU_PROGRAM_MAP(abc800m_map)
	MDRV_CPU_IO_MAP(abc800m_io_map)

	MDRV_MACHINE_START(abc800)
	MDRV_MACHINE_RESET(abc800)

	/* video hardware */
	MDRV_IMPORT_FROM(abc800m_video)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("discrete", DISCRETE, 0)
	MDRV_SOUND_CONFIG_DISCRETE(abc800)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	/* peripheral hardware */
	MDRV_Z80CTC_ADD(Z80CTC_TAG, ABC800_X01/2/2, ctc_intf)
	MDRV_TIMER_ADD_PERIODIC("ctc", ctc_tick, HZ(ABC800_X01/2/2/2))
	MDRV_Z80SIO_ADD(Z80SIO_TAG, ABC800_X01/2/2, sio_intf)
	MDRV_Z80DART_ADD(Z80DART_TAG, ABC800_X01/2/2, abc800_dart_intf)
//  MDRV_ABC77_ADD(abc800_abc77_intf)
	MDRV_PRINTER_ADD("printer")
	MDRV_CASSETTE_ADD(CASSETTE_TAG, default_cassette_config)

	/* ABC bus */
	MDRV_ABCBUS_ADD(ABCBUS_TAG, abcbus_daisy)
	MDRV_LUXOR_55_21046_ADD("abc830")

	/* fake keyboard */
	MDRV_TIMER_ADD_PERIODIC("keyboard", keyboard_tick, USEC(2500))

	/* internal ram */
	MDRV_RAM_ADD("messram")
	MDRV_RAM_DEFAULT_SIZE("16K")
	MDRV_RAM_EXTRA_OPTIONS("32K")
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( abc800c )
	MDRV_DRIVER_DATA(abc800_state)

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80_TAG, Z80, ABC800_X01/2/2)
	MDRV_CPU_CONFIG(abc800_daisy_chain)
	MDRV_CPU_PROGRAM_MAP(abc800c_map)
	MDRV_CPU_IO_MAP(abc800c_io_map)

	MDRV_MACHINE_START(abc800)
	MDRV_MACHINE_RESET(abc800)

	/* video hardware */
	MDRV_IMPORT_FROM(abc800c_video)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("discrete", DISCRETE, 0)
	MDRV_SOUND_CONFIG_DISCRETE(abc800)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	/* peripheral hardware */
	MDRV_Z80CTC_ADD(Z80CTC_TAG, ABC800_X01/2/2, ctc_intf)
	MDRV_TIMER_ADD_PERIODIC("ctc", ctc_tick, HZ(ABC800_X01/2/2/2))
	MDRV_Z80SIO_ADD(Z80SIO_TAG, ABC800_X01/2/2, sio_intf)
	MDRV_Z80DART_ADD(Z80DART_TAG, ABC800_X01/2/2, abc800_dart_intf)
//  MDRV_ABC77_ADD(abc800_abc77_intf)
	MDRV_PRINTER_ADD("printer")
	MDRV_CASSETTE_ADD(CASSETTE_TAG, default_cassette_config)

	/* ABC bus */
	MDRV_ABCBUS_ADD(ABCBUS_TAG, abcbus_daisy)
	MDRV_LUXOR_55_21046_ADD("abc830")

	/* fake keyboard */
	MDRV_TIMER_ADD_PERIODIC("keyboard", keyboard_tick, USEC(2500))

	/* internal ram */
	MDRV_RAM_ADD("messram")
	MDRV_RAM_DEFAULT_SIZE("16K")
	MDRV_RAM_EXTRA_OPTIONS("32K")
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( abc802 )
	MDRV_DRIVER_DATA(abc802_state)

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80_TAG, Z80, ABC800_X01/2/2)
	MDRV_CPU_CONFIG(abc800_daisy_chain)
	MDRV_CPU_PROGRAM_MAP(abc802_map)
	MDRV_CPU_IO_MAP(abc802_io_map)

	MDRV_MACHINE_START(abc802)
	MDRV_MACHINE_RESET(abc802)

	/* video hardware */
	MDRV_IMPORT_FROM(abc802_video)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("discrete", DISCRETE, 0)
	MDRV_SOUND_CONFIG_DISCRETE(abc800)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	/* peripheral hardware */
	MDRV_Z80CTC_ADD(Z80CTC_TAG, ABC800_X01/2/2, ctc_intf)
	MDRV_TIMER_ADD_PERIODIC("ctc", ctc_tick, HZ(ABC800_X01/2/2/2))
	MDRV_Z80SIO_ADD(Z80SIO_TAG, ABC800_X01/2/2, sio_intf)
	MDRV_Z80DART_ADD(Z80DART_TAG, ABC800_X01/2/2, abc802_dart_intf)
//  MDRV_ABC77_ADD(abc800_abc77_intf)
	MDRV_PRINTER_ADD("printer")
	MDRV_CASSETTE_ADD(CASSETTE_TAG, default_cassette_config)

	/* ABC bus */
	MDRV_ABCBUS_ADD(ABCBUS_TAG, abc802_abcbus_daisy)
	MDRV_LUXOR_55_21046_ADD("abc830")

	/* fake keyboard */
	MDRV_TIMER_ADD_PERIODIC("keyboard", keyboard_tick, USEC(2500))

	/* internal ram */
	MDRV_RAM_ADD("messram")
	MDRV_RAM_DEFAULT_SIZE("64K")
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( abc806 )
	MDRV_DRIVER_DATA(abc806_state)

	/* basic machine hardware */
	MDRV_CPU_ADD(Z80_TAG, Z80, ABC800_X01/2/2)
	MDRV_CPU_CONFIG(abc800_daisy_chain)
	MDRV_CPU_PROGRAM_MAP(abc806_map)
	MDRV_CPU_IO_MAP(abc806_io_map)

	MDRV_MACHINE_START(abc806)
	MDRV_MACHINE_RESET(abc806)

	/* video hardware */
	MDRV_IMPORT_FROM(abc806_video)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	/* peripheral hardware */
	MDRV_E0516_ADD(E0516_TAG, ABC806_X02)
	MDRV_Z80CTC_ADD(Z80CTC_TAG, ABC800_X01/2/2, ctc_intf)
	MDRV_TIMER_ADD_PERIODIC("ctc", ctc_tick, HZ(ABC800_X01/2/2/2))
	MDRV_Z80SIO_ADD(Z80SIO_TAG, ABC800_X01/2/2, sio_intf)
	MDRV_Z80DART_ADD(Z80DART_TAG, ABC800_X01/2/2, abc806_dart_intf)
//  MDRV_ABC77_ADD(abc800_abc77_intf)
	MDRV_PRINTER_ADD("printer")
	MDRV_CASSETTE_ADD(CASSETTE_TAG, default_cassette_config)

	/* ABC bus */
	MDRV_ABCBUS_ADD(ABCBUS_TAG, abcbus_daisy)
	MDRV_LUXOR_55_21046_ADD("abc830")

	/* fake keyboard */
	MDRV_TIMER_ADD_PERIODIC("keyboard", keyboard_tick, USEC(2500))

	/* internal ram */
	MDRV_RAM_ADD("messram")
	MDRV_RAM_DEFAULT_SIZE("160K") // 32KB + 128KB
	MDRV_RAM_EXTRA_OPTIONS("544K") // 32KB + 512KB
MACHINE_DRIVER_END

/* ROMs */

/*

    ABC800 DOS ROMs

    Label       Drive Type
    ----------------------------
    ABC 6-1X    ABC830,DD82,DD84
    800 8"      DD88
    ABC 6-2X    ABC832
    ABC 6-3X    ABC838
    ABC 6-52    ABC834
    UFD 6.XX    Winchester


    Floppy Controllers

    Art N/O
    --------
    55 10761-01     "old" controller
    55 10828-01     "old" controller
    55 20900-0x
    55 21046-11     Luxor Conkort   25 pin D-sub connector
    55 21046-21     Luxor Conkort   34 pin FDD connector
    55 21046-41     Luxor Conkort   both of the above

*/

ROM_START( abc800m )
	ROM_REGION( 0x10000, Z80_TAG, 0 )
	ROM_LOAD( "abc m-12.1m", 0x0000, 0x1000, CRC(f85b274c) SHA1(7d0f5639a528d8d8130a22fe688d3218c77839dc) )
	ROM_LOAD( "abc 1-12.1l", 0x1000, 0x1000, CRC(1e99fbdc) SHA1(ec6210686dd9d03a5ed8c4a4e30e25834aeef71d) )
	ROM_LOAD( "abc 2-12.1k", 0x2000, 0x1000, CRC(ac196ba2) SHA1(64fcc0f03fbc78e4c8056e1fa22aee12b3084ef5) )
	ROM_LOAD( "abc 3-12.1j", 0x3000, 0x1000, CRC(3ea2b5ee) SHA1(5a51ac4a34443e14112a6bae16c92b5eb636603f) )
	ROM_LOAD( "abc 4-12.2m", 0x4000, 0x1000, CRC(695cb626) SHA1(9603ce2a7b2d7b1cbeb525f5493de7e5c1e5a803) )
	ROM_LOAD( "abc 5-12.2l", 0x5000, 0x1000, CRC(b4b02358) SHA1(95338efa3b64b2a602a03bffc79f9df297e9534a) )
	ROM_LOAD( "abc 6-13.2k", 0x6000, 0x1000, CRC(6fa71fb6) SHA1(b037dfb3de7b65d244c6357cd146376d4237dab6) )
	ROM_LOAD( "abc 7-21.2j", 0x7000, 0x1000, CRC(fd137866) SHA1(3ac914d90db1503f61397c0ea26914eb38725044) )

	ROM_REGION( 0x800, "chargen", 0 )
	ROM_LOAD( "vu m-se.7c",  0x0000, 0x0800, CRC(f9152163) SHA1(997313781ddcbbb7121dbf9eb5f2c6b4551fc799) )

	ROM_REGION( 0x200, "fgctl", 0 )
	ROM_LOAD( "fgctl.bin",  0x0000, 0x0200, BAD_DUMP CRC(7a19de8d) SHA1(e7cc49e749b37f7d7dd14f3feda53eae843a8fe0) )
ROM_END

#define rom_abc800c rom_abc800m

ROM_START( abc802 )
	ROM_REGION( 0x10000, Z80_TAG, 0 )
	ROM_LOAD(  "abc 02-11.9f",  0x0000, 0x2000, CRC(b86537b2) SHA1(4b7731ef801f9a03de0b5acd955f1e4a1828355d) )
	ROM_LOAD(  "abc 12-11.11f", 0x2000, 0x2000, CRC(3561c671) SHA1(f12a7c0fe5670ffed53c794d96eb8959c4d9f828) )
	ROM_LOAD(  "abc 22-11.12f", 0x4000, 0x2000, CRC(8dcb1cc7) SHA1(535cfd66c84c0370fd022d6edf702d3d1ad1b113) )
	ROM_SYSTEM_BIOS( 0, "v19", "UDF-DOS v6.19" )
	ROMX_LOAD( "abc 32-21.14f", 0x6000, 0x2000, CRC(57050b98) SHA1(b977e54d1426346a97c98febd8a193c3e8259574), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "v20", "UDF-DOS v6.20" )
	ROMX_LOAD( "abc 32-31.14f", 0x6000, 0x2000, CRC(fc8be7a8) SHA1(a1d4cb45cf5ae21e636dddfa70c99bfd2050ad60), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "mica", "MICA DOS v6.20" )
	ROMX_LOAD( "mica820.14f",  0x6000, 0x2000, CRC(edf998af) SHA1(daae7e1ff6ef3e0ddb83e932f324c56f4a98f79b), ROM_BIOS(3) )

	ROM_REGION( 0x2000, "chargen", 0 )
	ROM_LOAD( "abc t2-11.3g",  0x0000, 0x2000, CRC(e21601ee) SHA1(2e838ebd7692e5cb9ba4e80fe2aa47ea2584133a) ) // 64 90191-01

	ROM_REGION( 0x400, "plds", 0 )
	ROM_LOAD( "abc p2-11.2g", 0x0000, 0x0400, NO_DUMP ) // PAL16R4
ROM_END

ROM_START( abc806 )
	ROM_REGION( 0x10000, Z80_TAG, 0 )
	ROM_LOAD( "abc 06-11.1m",  0x0000, 0x1000, CRC(27083191) SHA1(9b45592273a5673e4952c6fe7965fc9398c49827) ) // BASIC PROM ABC 06-11 "64 90231-02"
	ROM_LOAD( "abc 16-11.1l",  0x1000, 0x1000, CRC(eb0a08fd) SHA1(f0b82089c5c8191fbc6a3ee2c78ce730c7dd5145) ) // BASIC PROM ABC 16-11 "64 90232-02"
	ROM_LOAD( "abc 26-11.1k",  0x2000, 0x1000, CRC(97a95c59) SHA1(013bc0a2661f4630c39b340965872bf607c7bd45) ) // BASIC PROM ABC 26-11 "64 90233-02"
	ROM_LOAD( "abc 36-11.1j",  0x3000, 0x1000, CRC(b50e418e) SHA1(991a59ed7796bdcfed310012b2bec50f0b8df01c) ) // BASIC PROM ABC 36-11 "64 90234-02"
	ROM_LOAD( "abc 46-11.2m",  0x4000, 0x1000, CRC(17a87c7d) SHA1(49a7c33623642b49dea3d7397af5a8b9dde8185b) ) // BASIC PROM ABC 46-11 "64 90235-02"
	ROM_LOAD( "abc 56-11.2l",  0x5000, 0x1000, CRC(b4b02358) SHA1(95338efa3b64b2a602a03bffc79f9df297e9534a) ) // BASIC PROM ABC 56-11 "64 90236-02"
	ROM_SYSTEM_BIOS( 0, "v19",		"UDF-DOS v.19" )
	ROMX_LOAD( "abc 66-21.2k", 0x6000, 0x1000, CRC(c311b57a) SHA1(4bd2a541314e53955a0d53ef2f9822a202daa485), ROM_BIOS(1) ) // DOS PROM ABC 66-21 "64 90314-01"
	ROM_SYSTEM_BIOS( 1, "v20",		"UDF-DOS v.20" )
	ROMX_LOAD( "abc 66-31.2k", 0x6000, 0x1000, CRC(a2e38260) SHA1(0dad83088222cb076648e23f50fec2fddc968883), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "mica",		"MICA DOS v.20" )
	ROMX_LOAD( "mica2006.2k", 0x6000, 0x1000, CRC(58bc2aa8) SHA1(0604bd2396f7d15fcf3d65888b4b673f554037c0), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "catnet",	"CAT-NET" )
	ROMX_LOAD( "cmd8_5.2k",	  0x6000, 0x1000, CRC(25430ef7) SHA1(03a36874c23c215a19b0be14ad2f6b3b5fb2c839), ROM_BIOS(4) )
	ROM_LOAD( "abc 76-11.2j",  0x7000, 0x1000, CRC(3eb5f6a1) SHA1(02d4e38009c71b84952eb3b8432ad32a98a7fe16) ) // Options-PROM ABC 76-11 "64 90238-02"

	ROM_REGION( 0x1000, "chargen", 0 )
	ROM_LOAD( "abc t6-11.7c",   0x0000, 0x1000, CRC(b17c51c5) SHA1(e466e80ec989fbd522c89a67d274b8f0bed1ff72) ) // 64 90243-01

	ROM_REGION( 0x200, "rad", 0 )
	ROM_LOAD( "64 90241-01.9b",  0x0000, 0x0200, NO_DUMP ) // "RAD" 7621/7643 (82S131/82S137), character line address

	ROM_REGION( 0x20, "hru", 0 )
	ROM_LOAD( "64 90128-01.6e",  0x0000, 0x0020, NO_DUMP ) // "HRU I" 7603 (82S123), HR horizontal timing and video memory access

	ROM_REGION( 0x200, "hru2", 0 )
	ROM_LOAD( "fgctl.bin",  0x0000, 0x0200, BAD_DUMP CRC(7a19de8d) SHA1(e7cc49e749b37f7d7dd14f3feda53eae843a8fe0) )
//  ROM_LOAD( "64 90127-01.12g", 0x0000, 0x0200, NO_DUMP ) // "HRU II" 7621 (82S131), ABC800C HR compatibility mode palette

	ROM_REGION( 0x400, "v50", 0 )
	ROM_LOAD( "64 90242-01.7e",  0x0000, 0x0200, NO_DUMP ) // "V50" 7621 (82S131), HR vertical timing 50Hz
	ROM_LOAD( "64 90319-01.7e",  0x0200, 0x0200, NO_DUMP ) // "V60" 7621 (82S131), HR vertical timing 60Hz

	ROM_REGION( 0x400, "plds", 0 )
	ROM_LOAD( "64 90225-01.11c", 0x0000, 0x0400, NO_DUMP ) // "VIDEO ATTRIBUTE" 40033A (?)
	ROM_LOAD( "64 90239-01.1b", 0x0000, 0x0400, NO_DUMP ) // "ABC P3-11" PAL16R4, color encoder
	ROM_LOAD( "64 90240-01.2d", 0x0000, 0x0400, NO_DUMP ) // "ABC P4-11" PAL16L8, memory mapper
ROM_END

/* Driver Initialization */

DIRECT_UPDATE_HANDLER( abc800_direct_update_handler )
{
	abc800_state *state = machine->driver_data<abc800_state>();

	if (address >= 0x7800 && address < 0x8000)
	{
		direct.explicit_configure(0x7800, 0x7fff, 0x7ff, *direct.space().m_machine.region(Z80_TAG));

		if (!state->fetch_charram)
		{
			state->fetch_charram = 1;
			abc800_bankswitch(machine);
		}

		return ~0;
	}

	if (state->fetch_charram)
	{
		state->fetch_charram = 0;
		abc800_bankswitch(machine);
	}

	return address;
}

DIRECT_UPDATE_HANDLER( abc802_direct_update_handler )
{
	abc802_state *state = machine->driver_data<abc802_state>();

	if (state->lrs)
	{
		if (address >= 0x7800 && address < 0x8000)
		{
			direct.explicit_configure(0x7800, 0x7fff, 0x7ff, *direct.space().m_machine.region(Z80_TAG));
			return ~0;
		}
	}
	else
	{
		direct.explicit_configure(0x0000, 0xffff, 0xffff, messram_get_ptr(machine->device("messram")));
		return ~0;
	}

	return address;
}

DIRECT_UPDATE_HANDLER( abc806_direct_update_handler )
{
	abc806_state *state = machine->driver_data<abc806_state>();

	if (address >= 0x7800 && address < 0x8000)
	{
		direct.explicit_configure(0x7800, 0x7fff, 0x7ff, *direct.space().m_machine.region(Z80_TAG));

		if (!state->fetch_charram)
		{
			state->fetch_charram = 1;
			abc806_bankswitch(machine);
		}

		return ~0;
	}

	if (state->fetch_charram)
	{
		state->fetch_charram = 0;
		abc806_bankswitch(machine);
	}

	return address;
}

static DRIVER_INIT( abc800 )
{
	address_space *program = machine->device<cpu_device>(Z80_TAG)->space(AS_PROGRAM);
	program->set_direct_update_handler(direct_update_delegate_create_static(abc800_direct_update_handler, *machine));
}

static DRIVER_INIT( abc802 )
{
	address_space *program = machine->device<cpu_device>(Z80_TAG)->space(AS_PROGRAM);
	program->set_direct_update_handler(direct_update_delegate_create_static(abc802_direct_update_handler, *machine));
}

static DRIVER_INIT( abc806 )
{
	address_space *program = machine->device<cpu_device>(Z80_TAG)->space(AS_PROGRAM);
	program->set_direct_update_handler(direct_update_delegate_create_static(abc806_direct_update_handler, *machine));
}

/* System Drivers */

/*    YEAR  NAME        PARENT      COMPAT  MACHINE     INPUT   INIT    COMPANY             FULLNAME        FLAGS */
COMP( 1981, abc800m,    0,			0,      abc800m,    abc800, abc800, "Luxor Datorer AB", "ABC 800 M/HR", GAME_IMPERFECT_GRAPHICS | GAME_IMPERFECT_SOUND )
COMP( 1981, abc800c,    abc800m,    0,      abc800c,    abc800, abc800, "Luxor Datorer AB", "ABC 800 C/HR", GAME_NOT_WORKING )
COMP( 1983, abc802,     0,          0,      abc802,     abc802, abc802, "Luxor Datorer AB", "ABC 802",		GAME_IMPERFECT_GRAPHICS | GAME_IMPERFECT_SOUND )
COMP( 1983, abc806,     0,          0,      abc806,     abc806, abc806, "Luxor Datorer AB", "ABC 806",		GAME_IMPERFECT_GRAPHICS | GAME_IMPERFECT_SOUND | GAME_NO_SOUND)
