// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic, Jonathan Gevaryahu
/***************************************************************************

        DEC VT240

        31/03/2010 Skeleton driver.

    TODO:
    - understand how PCG works, it should be a funky i/o $30 to uPD7220 DMA
      transfer;
    - hook-up T11, rst65 irq + $20 reads are latches for that

    ROM POST notes:
    0x0139: ROM test
    0x015f: RAM test
    0x0071: RAM fill to 0x00
    0x1c8f: UPD7220

    // vt240: x2212 nvram at E56
****************************************************************************/

#include "emu.h"

#include "bus/rs232/rs232.h"
#include "cpu/i8085/i8085.h"
#include "cpu/t11/t11.h"
#include "machine/clock.h"
#include "machine/dec_lk201.h"
#include "machine/i8251.h"
#include "machine/mc68681.h"
#include "machine/ms7004.h"
#include "machine/bankdev.h"
#include "video/upd7220.h"

#define VERBOSE_DBG 1       /* general debug messages */

#define DBG_LOG(N,M,A) \
	do { \
	if(VERBOSE_DBG>=N) \
		{ \
			logerror("%11.6f at %s: ",machine().time().as_double(),machine().describe_context()); \
			logerror A; \
		} \
	} while (0)

class vt240_state : public driver_device
{
public:
	vt240_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_i8085(*this, "charcpu"),
		m_i8251(*this, "i8251"),
		m_duart(*this, "duart"),
		m_hgdc(*this, "upd7220"),
		m_bank(*this, "bank"),
		m_video_ram(*this, "video_ram"),
		m_rom(*this, "maincpu"){ }

	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_i8085;
	required_device<i8251_device> m_i8251;
	required_device<mc68681_device> m_duart;
	required_device<upd7220_device> m_hgdc;
	required_device<address_map_bank_device> m_bank;
	required_shared_ptr<UINT16> m_video_ram;
	required_region_ptr<UINT16> m_rom;

	DECLARE_WRITE_LINE_MEMBER(write_keyboard_clock);
	DECLARE_WRITE_LINE_MEMBER(i8085_rdy_w);
	DECLARE_READ8_MEMBER( test_r );
	DECLARE_READ8_MEMBER(i8085_comm_r);
	DECLARE_WRITE8_MEMBER(i8085_comm_w);
	DECLARE_READ8_MEMBER(t11_comm_r);
	DECLARE_WRITE8_MEMBER(t11_comm_w);
	DECLARE_READ8_MEMBER(mem_map_cs_r);
	DECLARE_WRITE8_MEMBER(mem_map_cs_w);
	DECLARE_READ8_MEMBER(ctrl_r);
	DECLARE_WRITE8_MEMBER(mem_map_sel_w);
	DECLARE_READ16_MEMBER(mem_r);
	DECLARE_WRITE16_MEMBER(mem_w);

	//UINT16 m_pcg_addr;
	//UINT8 m_pcg_internal_addr;
	//UINT8 *m_char_rom;

	DECLARE_DRIVER_INIT(vt240);
	virtual void machine_reset() override;
	UPD7220_DRAW_TEXT_LINE_MEMBER( hgdc_draw_text );

	UINT8 m_i8085_out, m_t11_out;
	UINT8 m_mem_map[16];
	UINT8 m_mem_map_sel;
};

WRITE_LINE_MEMBER(vt240_state::write_keyboard_clock)
{
	m_i8251->write_txc(state);
	m_i8251->write_rxc(state);
}

WRITE_LINE_MEMBER(vt240_state::i8085_rdy_w)
{
	//m_maincpu->set_input_line(T11_IRQ1, state ? ASSERT_LINE : CLEAR_LINE);
}

/* TODO */
UPD7220_DRAW_TEXT_LINE_MEMBER( vt240_state::hgdc_draw_text )
{
	//int x;
	//int xi,yi;
	//int tile,color;
	//UINT8 tile_data;

	#if 0
	for( x = 0; x < pitch; x++ )
	{
		tile = (vram[(addr+x)*2] & 0xff);
		color = (vram[(addr+x)*2+1] & 0x0f);

		for( yi = 0; yi < lr; yi++)
		{
			tile_data = m_char_rom[(tile*8+yi) & 0x7ff];

			if(cursor_on && cursor_addr == addr+x) //TODO
				tile_data^=0xff;

			for( xi = 0; xi < 8; xi++)
			{
				int res_x,res_y;
				int pen = (tile_data >> xi) & 1 ? color : 0;

				if(yi >= 8) { pen = 0; }

				res_x = x * 8 + xi;
				res_y = y + yi;

				if(res_x > screen_max_x || res_y > screen_max_y)
					continue;

				bitmap.pix16(res_y, res_x) = pen;
			}
		}
	}
	#endif
}



READ8_MEMBER(vt240_state::t11_comm_r)
{
	return m_t11_out;
}

WRITE8_MEMBER(vt240_state::t11_comm_w)
{
	m_i8085_out = data;
}

READ8_MEMBER(vt240_state::i8085_comm_r)
{
	switch(offset)
	{
		case 0:
			m_i8085->set_input_line(I8085_RST65_LINE, CLEAR_LINE);
			return m_i8085_out;
	}
	return 0xff;
}

WRITE8_MEMBER(vt240_state::i8085_comm_w)
{
	switch(offset)
	{
		case 1:
			m_t11_out = data;
			m_i8085->set_input_line(I8085_RST65_LINE, ASSERT_LINE);
			break;
		case 2:
			m_i8085->set_input_line(INPUT_LINE_RESET, PULSE_LINE);
			break;
	}
}
READ8_MEMBER(vt240_state::mem_map_cs_r)
{
	return ~m_mem_map[offset];
}

WRITE8_MEMBER(vt240_state::mem_map_cs_w)
{
	m_mem_map[offset] = ~data;
}

READ8_MEMBER(vt240_state::ctrl_r)
{
	return m_mem_map_sel;
}

WRITE8_MEMBER(vt240_state::mem_map_sel_w)
{
	m_mem_map_sel = data & 1;
}

READ16_MEMBER(vt240_state::mem_r)
{
	if(m_mem_map_sel)
	{
		m_bank->set_bank(m_mem_map[(offset >> 11) & 0xf]);
		return m_bank->read16(space, offset & 0x7ff, mem_mask);
	}
	else
		return m_rom[offset];
}

WRITE16_MEMBER(vt240_state::mem_w)
{
	if(m_mem_map_sel)
	{
		m_bank->set_bank(m_mem_map[(offset >> 11) & 0xf]);
		m_bank->write16(space, offset & 0x7ff, data, mem_mask);
	}
}

static ADDRESS_MAP_START(bank_map, AS_PROGRAM, 16, vt240_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00000, 0x0ffff) AM_ROM AM_REGION("maincpu", 0)
	AM_RANGE(0x80000, 0x87fff) AM_RAM
ADDRESS_MAP_END

// PDF page 78 (4-25)
static ADDRESS_MAP_START( vt240_mem, AS_PROGRAM, 16, vt240_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE (0000000, 0167777) AM_READWRITE(mem_r, mem_w)
	AM_RANGE (0170000, 0170037) AM_READWRITE8(mem_map_cs_r, mem_map_cs_w, 0x00ff)
	AM_RANGE (0170040, 0170041) AM_WRITE8(mem_map_sel_w, 0x00ff)
	AM_RANGE (0170100, 0170101) AM_READ8(ctrl_r, 0x00ff)
	AM_RANGE (0171000, 0171003) AM_DEVREADWRITE8("i8251", i8251_device, data_r, data_w, 0x00ff)
	AM_RANGE (0171004, 0171007) AM_DEVREADWRITE8("i8251", i8251_device, status_r, control_w, 0x00ff)
	AM_RANGE (0172000, 0172077) AM_DEVREADWRITE8("duart", mc68681_device, read, write, 0xff)
	// 0173000 Video logic
	// 0174000 Video logic
	AM_RANGE (0175000, 0175005) AM_READWRITE8(i8085_comm_r, i8085_comm_w, 0x00ff)
	// 0176xxx NVR
	// 017700x System comm logic
ADDRESS_MAP_END

// PDF page 134 (6-9)
static ADDRESS_MAP_START(vt240_char_mem, AS_PROGRAM, 8, vt240_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x3fff) AM_ROM AM_REGION("charcpu", 0)
	AM_RANGE(0x4000, 0x5fff) AM_ROM AM_REGION("charcpu", 0x8000)
	AM_RANGE(0x8000, 0x87ff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START(vt240_char_io, AS_IO, 8, vt240_state)
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x01) AM_DEVREADWRITE("upd7220", upd7220_device, read, write)
	AM_RANGE(0x20, 0x20) AM_READWRITE(t11_comm_r, t11_comm_w)
	//AM_RANGE(0x30, 0x30) AM_READWRITE(pcg_r,pcg_w) // 0x30 PCG
ADDRESS_MAP_END

static ADDRESS_MAP_START( upd7220_map, AS_0, 16, vt240_state)
	AM_RANGE(0x00000, 0x0ffff) AM_RAM AM_SHARE("video_ram")
ADDRESS_MAP_END


void vt240_state::machine_reset()
{
	m_i8251->write_cts(0);
	m_mem_map_sel = 0;
}

static const gfx_layout vt240_chars_8x10 =
{
	8,10,
	RGN_FRAC(1,1),
	1,
	{ 0 },
	{ STEP8(0,1) },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8 },
	8*10
};

static GFXDECODE_START( vt240 )
	GFXDECODE_ENTRY( "charcpu", 0x338*10-2, vt240_chars_8x10, 0, 8 )
GFXDECODE_END

static MACHINE_CONFIG_START( vt240, vt240_state )
	MCFG_CPU_ADD("maincpu", T11, XTAL_7_3728MHz) // confirm
	MCFG_CPU_PROGRAM_MAP(vt240_mem)
	MCFG_T11_INITIAL_MODE(5 << 13)

	MCFG_CPU_ADD("charcpu", I8085A, XTAL_16MHz / 4)
	MCFG_CPU_PROGRAM_MAP(vt240_char_mem)
	MCFG_CPU_IO_MAP(vt240_char_io)
	MCFG_I8085A_SOD(WRITELINE(vt240_state, i8085_rdy_w))

	MCFG_DEVICE_ADD("bank", ADDRESS_MAP_BANK, 0)
	MCFG_DEVICE_PROGRAM_MAP(bank_map)
	MCFG_ADDRESS_MAP_BANK_ENDIANNESS(ENDIANNESS_LITTLE)
	MCFG_ADDRESS_MAP_BANK_ADDRBUS_WIDTH(20)
	MCFG_ADDRESS_MAP_BANK_DATABUS_WIDTH(16)
	MCFG_ADDRESS_MAP_BANK_STRIDE(0x1000)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(640, 480)
	MCFG_SCREEN_VISIBLE_AREA(0, 640-1, 0, 480-1)
//  MCFG_VIDEO_START_OVERRIDE(vt240_state,vt240)
	MCFG_SCREEN_UPDATE_DEVICE("upd7220", upd7220_device, screen_update)
	MCFG_PALETTE_ADD_MONOCHROME("palette")
	MCFG_GFXDECODE_ADD("gfxdecode", "palette", vt240)

	MCFG_DEVICE_ADD("upd7220", UPD7220, XTAL_4MHz / 4)
	MCFG_DEVICE_ADDRESS_MAP(AS_0, upd7220_map)
	MCFG_UPD7220_DRAW_TEXT_CALLBACK_OWNER(vt240_state, hgdc_draw_text)

	MCFG_MC68681_ADD("duart", XTAL_3_6864MHz) /* 2681 duart (not 68681!) */
//  MCFG_MC68681_IRQ_CALLBACK(WRITELINE(dectalk_state, dectalk_duart_irq_handler))
	MCFG_MC68681_A_TX_CALLBACK(DEVWRITELINE("rs232", rs232_port_device, write_txd))
//  MCFG_MC68681_B_TX_CALLBACK(WRITELINE(dectalk_state, dectalk_duart_txa))
//  MCFG_MC68681_INPORT_CALLBACK(READ8(dectalk_state, dectalk_duart_input))
//  MCFG_MC68681_OUTPORT_CALLBACK(WRITE8(dectalk_state, dectalk_duart_output))
//  MCFG_I8251_DTR_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_dtr))
//  MCFG_I8251_RTS_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_rts))

	MCFG_DEVICE_ADD("i8251", I8251, 0)
	MCFG_I8251_TXD_HANDLER(DEVWRITELINE("lk201", lk201_device, rx_w))
	//MCFG_I8251_DTR_HANDLER(WRITELINE(rainbow_state, irq_hi_w))
	//MCFG_I8251_RXRDY_HANDLER(INPUTLINE("maincpu", ))
	//MCFG_I8251_TXRDY_HANDLER(WRITELINE(rainbow_state, kbd_txready_w))

	MCFG_DEVICE_ADD("lk201", LK201, 0)
	MCFG_LK201_TX_HANDLER(DEVWRITELINE("i8251", i8251_device, write_rxd))

	MCFG_DEVICE_ADD("keyboard_clock", CLOCK, 4800 * 64) // 8251 is set to /64 on the clock input
	MCFG_CLOCK_SIGNAL_HANDLER(WRITELINE(vt240_state, write_keyboard_clock))

	MCFG_RS232_PORT_ADD("rs232", default_rs232_devices, "null_modem")
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("duart", mc68681_device, rx_a_w))
//  MCFG_RS232_DSR_HANDLER(DEVWRITELINE("duart", mc68681_device, ipX_w))
MACHINE_CONFIG_END

static MACHINE_CONFIG_DERIVED( mc7105, vt240 )

	MCFG_DEVICE_REMOVE("lk201")
	MCFG_DEVICE_ADD("ms7004", MS7004, 0)
	MCFG_MS7004_TX_HANDLER(DEVWRITELINE("i8251", i8251_device, write_rxd))

	MCFG_DEVICE_MODIFY("i8251")
	MCFG_I8251_TXD_HANDLER(NOOP)
	//MCFG_I8251_TXD_HANDLER(DEVWRITELINE("ms7004", ms7004_device, rx_w))

	// baud rate is supposed to be 4800 but keyboard is slightly faster
	MCFG_DEVICE_REMOVE("keyboard_clock")
	MCFG_DEVICE_ADD("keyboard_clock", CLOCK, 4960*16)
	MCFG_CLOCK_SIGNAL_HANDLER(WRITELINE(vt240_state, write_keyboard_clock))
MACHINE_CONFIG_END

/* ROM definition */
ROM_START( mc7105 )
	ROM_REGION( 0x10000, "charcpu", ROMREGION_ERASEFF )
	ROM_LOAD( "027.bin", 0x8000, 0x8000, CRC(a159b412) SHA1(956097ccc2652d494258b3682498cfd3096d7d4f))
	ROM_LOAD( "028.bin", 0x0000, 0x8000, CRC(b253151f) SHA1(22ffeef8eb5df3c38bfe91266f26d1e7822cdb53))

	ROM_REGION( 0x40000, "maincpu", ROMREGION_ERASEFF )
	ROM_LOAD16_BYTE( "029.bin", 0x00000, 0x8000, CRC(4a6db217) SHA1(47637325609ea19ffab61fe31e2700d72fa50729))
	ROM_LOAD16_BYTE( "031.bin", 0x00001, 0x8000, CRC(47129579) SHA1(39de9e2e26f90c5da5e72a09ff361c1a94b9008a))
	ROM_LOAD16_BYTE( "030.bin", 0x10000, 0x8000, CRC(05fd7b75) SHA1(2ad8c14e76accfa1b9b8748c58e9ebbc28844a47))
	ROM_LOAD16_BYTE( "032.bin", 0x10001, 0x8000, CRC(e81d93c4) SHA1(982412a7a6e65d6f6b4f66bd093e54ee16f31384))
ROM_END

/* ROM definition */
ROM_START( vt240 )
	ROM_REGION( 0x10000, "charcpu", ROMREGION_ERASEFF )
	ROM_LOAD( "23-008e6-00.e100", 0x0000, 0x8000, CRC(ebc8a2fe) SHA1(70838175f8302fdc0dee79b2403fa95e6d989206))

	ROM_REGION( 0x40000, "maincpu", ROMREGION_ERASEFF )
	ROM_DEFAULT_BIOS( "vt240" )
	// according to the schematics an even older set exists, variation 'E1' with roms:
	// e100/8085: 23-003e6
	// e20: 23-001e6
	// e22: 23-002e6
	// e19: 23-048e5
	// e21: 23-049e5
	// but according to the Field Change Order below, the initial release is V2.1, so the above must be a prototype.
	// DOL for v2.1 to v2.2 change: http://web.archive.org/web/20060905145200/http://cmcnabb.cc.vt.edu/dec94mds/vt240dol.txt
	ROM_SYSTEM_BIOS( 0, "vt240v21", "VT240 V2.1" ) // initial factory release, FCO says this was 8 Feburary 1985
	ROMX_LOAD( "23-006e6-00.e20", 0x00000, 0x8000, CRC(79c11d82) SHA1(5a6fe5b75b6504a161f2c9b148c0fe9f19770837), ROM_SKIP(1) | ROM_BIOS(1))
	ROMX_LOAD( "23-004e6-00.e22", 0x00001, 0x8000, CRC(eba10fef) SHA1(c0ee4d8e4eeb70066f03f3d17a7e2f2bd0b5f8ad), ROM_SKIP(1) | ROM_BIOS(1))
	ROMX_LOAD( "23-007e6-00.e19", 0x10000, 0x8000, CRC(d18a2ab8) SHA1(37f448a332fc50298007ed39c8bf1ab1eb6d4cae), ROM_SKIP(1) | ROM_BIOS(1))
	ROMX_LOAD( "23-005e6-00.e21", 0x10001, 0x8000, CRC(558d0285) SHA1(e96a49bf9d55d8ab879d9b39aa380368c5c9ade0), ROM_SKIP(1) | ROM_BIOS(1))
	ROM_SYSTEM_BIOS( 1, "vt240", "VT240 V2.2" ) // Revised version, December 1985
	ROMX_LOAD( "23-058e6.e20", 0x00000, 0x8000, CRC(d2a56b90) SHA1(39cbb26134d7d8ba308df3a93228918a5945b45f), ROM_SKIP(1) | ROM_BIOS(2))
	ROMX_LOAD( "23-056e6.e22", 0x00001, 0x8000, CRC(c46e13c3) SHA1(0f2801fa7483d1f97708143cd81ae0816bf9a435), ROM_SKIP(1) | ROM_BIOS(2))
	ROMX_LOAD( "23-059e6.e19", 0x10000, 0x8000, CRC(f8393346) SHA1(1e28daf1b7f2bdabc47ce2f6fa99ef038b275a29), ROM_SKIP(1) | ROM_BIOS(2))
	ROMX_LOAD( "23-057e6.e21", 0x10001, 0x8000, CRC(7ce9dce9) SHA1(5a105e5bdca13910b3b79cc23567ce2dc36b844d), ROM_SKIP(1) | ROM_BIOS(2))
	// E39, E85, E131 are empty.

	ROM_REGION( 0x1000, "proms", ROMREGION_ERASEFF )
	ROM_LOAD( "23-351a1.e149", 0x0000, 0x0020, NO_DUMP) // 82s123; DRAM RAS/CAS Timing PROM
	ROM_LOAD( "23-352a1.e187", 0x0020, 0x0020, NO_DUMP) // 82s123; "CT0" Timing PROM
	ROM_LOAD( "23-369a1.e53", 0x0040, 0x0020, NO_DUMP) // 82s123; ROM and RAM mapping PROM
	ROM_LOAD( "23-370a1.e188", 0x0060, 0x0020, NO_DUMP) // 82s123; "CT1" Timing PROM
	ROM_LOAD( "23-994a9.e74", 0x0100, 0x0200, NO_DUMP) // 82s131; T11 Interrupt Encoder PROM

	ROM_REGION( 0x1000, "pals", 0 )
	ROM_LOAD( "23-087j5.e182.e183.jed", 0x0000, 0x1000, NO_DUMP ) // PAL16L8ACN; "Logic Unit" Character Pattern Related
ROM_END

/* Driver */
DRIVER_INIT_MEMBER(vt240_state,vt240)
{
	UINT8 *ROM = memregion("charcpu")->base();

	/* patch T11 check */
	ROM[0x09d] = 0x00;
	ROM[0x09e] = 0x00;
	ROM[0x09f] = 0x00;

	/* ROM checksum */
	ROM[0x15c] = 0x00;
	ROM[0x15d] = 0x00;
	ROM[0x15e] = 0x00;
}

/*    YEAR  NAME    PARENT  COMPAT   MACHINE    INPUT          CLASS   INIT    COMPANY                      FULLNAME       FLAGS */
COMP( 1983, vt240,  0,      0,       vt240,    0, vt240_state,   vt240,  "Digital Equipment Corporation", "VT240", MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
//COMP( 1983, vt241,  0,      0,       vt220,     vt220, driver_device,   0,  "Digital Equipment Corporation", "VT241", MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
// NOTE: the only difference between VT240 and VT241 is the latter comes with a VR241 Color monitor, while the former comes with a mono display; the ROMs and operation are identical.
COMP( 1983, mc7105, 0,      0,       mc7105,    0, vt240_state,   vt240,  "Elektronika",                  "MC7105", MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
