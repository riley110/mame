#include "emu.h"
#include "m68705.h"
#include "m6805defs.h"

namespace {

std::pair<u16, char const *> const m68705p_syms[] = {
	{ 0x0000, "PORTA" }, { 0x0001, "PORTB" }, { 0x0002, "PORTC" },
	{ 0x0004, "DDRA"  }, { 0x0005, "DDRB"  }, { 0x0006, "DDRC"  },
	{ 0x0008, "TDR"   }, { 0x0009, "TCR"   },
	{ 0x000b, "PCR"   },
	{ 0x0784, "MOR"   } };

std::pair<u16, char const *> const m68705r_syms[] = {
	{ 0x0000, "PORTA" }, { 0x0001, "PORTB" }, { 0x0002, "PORTC" }, { 0x0003, "PORTD" },
	{ 0x0004, "DDRA"  }, { 0x0005, "DDRB"  }, { 0x0006, "DDRC"  },
	{ 0x0008, "TDR"   }, { 0x0009, "TCR"   },
	{ 0x000a, "MISC"  },
	{ 0x000b, "PCR"   },
	{ 0x000e, "ACR"   }, { 0x000f, "ARR"   },
	{ 0x0f38, "MOR"   } };

std::pair<u16, char const *> const m68705u_syms[] = {
	{ 0x0000, "PORTA" }, { 0x0001, "PORTB" }, { 0x0002, "PORTC" }, { 0x0003, "PORTD" },
	{ 0x0004, "DDRA"  }, { 0x0005, "DDRB"  }, { 0x0006, "DDRC"  },
	{ 0x0008, "TDR"   }, { 0x0009, "TCR"   },
	{ 0x000a, "MISC"  },
	{ 0x000b, "PCR"   },
	{ 0x0f38, "MOR"   } };


ROM_START( m68705p3 )
	ROM_REGION(0x0073, "bootstrap", 0)
	ROM_LOAD("bootstrap.bin", 0x0000, 0x0073, CRC(696e1383) SHA1(45104fe1dbd683d251ed2b9411b1f4befbb5aff4))
ROM_END

ROM_START( m68705p5 )
	ROM_REGION(0x0073, "bootstrap", 0)
	ROM_LOAD("bootstrap.bin", 0x0000, 0x0073, CRC(f70a8620) SHA1(c154f78c23f10bb903a531cb19e99121d5f7c19c))
ROM_END

ROM_START( m68705r3 )
	ROM_REGION(0x0078, "bootstrap", 0)
	ROM_LOAD("bootstrap.bin", 0x0000, 0x0078, CRC(5946479b) SHA1(834ea00aef5de12dbcd6421a6e21d5ea96cfbf37) BAD_DUMP)
ROM_END

ROM_START( m68705u3 )
	ROM_REGION(0x0078, "bootstrap", 0)
	ROM_LOAD("bootstrap.bin", 0x0000, 0x0078, CRC(5946479b) SHA1(834ea00aef5de12dbcd6421a6e21d5ea96cfbf37))
ROM_END

} // anonymous namespace


device_type const M68705 = &device_creator<m68705_device>;
device_type const M68705P3 = &device_creator<m68705p3_device>;
device_type const M68705P5 = &device_creator<m68705p5_device>;
device_type const M68705R3 = &device_creator<m68705r3_device>;
device_type const M68705U3 = &device_creator<m68705u3_device>;


/****************************************************************************
 * M68705 device (no peripherals)
 ****************************************************************************/

m68705_device::m68705_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: m6805_base_device(mconfig, tag, owner, clock, M68705, "M68705", 12, "m68705", __FILE__)
{
}

m68705_device::m68705_device(
		machine_config const &mconfig,
		char const *tag,
		device_t *owner,
		u32 clock,
		device_type type,
		char const *name,
		u32 addr_width,
		address_map_delegate internal_map,
		char const *shortname,
		char const *source)
	: m6805_base_device(mconfig, tag, owner, clock, type, name, addr_width, internal_map, shortname, source)
{
}

/* Generate interrupt - m68705 version */
void m68705_device::interrupt()
{
	if ((m_pending_interrupts & ((1 << M6805_IRQ_LINE) | M68705_INT_MASK)) != 0 )
	{
		if ((CC & IFLAG) == 0)
		{
			PUSHWORD(m_pc);
			PUSHBYTE(m_x);
			PUSHBYTE(m_a);
			PUSHBYTE(m_cc);
			SEI;
			standard_irq_callback(0);

			if ((m_pending_interrupts & (1 << M68705_IRQ_LINE)) != 0 )
			{
				m_pending_interrupts &= ~(1 << M68705_IRQ_LINE);
				RM16(0xfffa, &m_pc);
			}
			else if ((m_pending_interrupts & (1 << M68705_INT_TIMER)) != 0)
			{
				m_pending_interrupts &= ~(1 << M68705_INT_TIMER);
				RM16(0xfff8, &m_pc);
			}
		}
		m_icount -= 11;
	}
}

void m68705_device::device_reset()
{
	m6805_base_device::device_reset();

	RM16(0xfffe, &m_pc);
}

void m68705_device::execute_set_input(int inputnum, int state)
{
	if (m_irq_state[inputnum] != state)
	{
		m_irq_state[inputnum] = (state == ASSERT_LINE) ? ASSERT_LINE : CLEAR_LINE;

		if (state != CLEAR_LINE)
			m_pending_interrupts |= 1 << inputnum;
	}
}


/****************************************************************************
 * M68705 "new" device
 ****************************************************************************/

/*
The 68(7)05 peripheral memory map:
Common for Px, Rx, Ux parts:
0x00: Port A data (RW)
0x01: Port B data (RW)
0x02: Port C data (RW) [top 4 bits do nothing (read as 1s) on Px parts, work as expected on Rx, Ux parts]
0x03: [Port D data (Read only), only on Rx, Ux parts]
0x04: Port A DDR (Write only, reads as 0xFF)
0x05: Port B DDR (Write only, reads as 0xFF)
0x06: Port C DDR (Write only, reads as 0xFF) [top 4 bits do nothing on Px parts, work as expected on Rx, Ux parts]
0x07: Unused (reads as 0xFF?)
0x08: Timer Data Register (RW; acts as ram when timer isn't counting, otherwise decrements once per prescaler expiry)
0x09: Timer Control Register (RW; on certain mask part and when MOR bit 6 is not set, all bits are RW except bit 3 which
always reads as zero. when MOR bit 6 is set and on all mask parts except one listed in errata in the 6805 daatsheet,
the top two bits are RW, bottom 6 always read as 1 and writes do nothing; on the errata chip, bit 3 is writable and
clears the prescaler, reads as zero)
0x0A: [Miscellaneous Register, only on Rx, Sx, Ux parts]
0x0B: [Eprom parts: Programming Control Register (write only?, low 3 bits; reads as 0xFF?); Unused (reads as 0xFF?) on
Mask parts]
0x0C: Unused (reads as 0xFF?)
0x0D: Unused (reads as 0xFF?)
0x0E: [A/D control register, only on Rx, Sx parts]
0x0F: [A/D result register, only on Rx, Sx parts]
0x10-0x7f: internal ram; SP can only point to 0x60-0x7F. U2/R2 parts have an unused hole from 0x10-0x3F (reads as 0xFF?)
0x80-0xFF: Page 0 user rom
The remainder of the memory map differs here between parts, see appropriate datasheet for each part.
The four vectors are always stored in big endian form as the last 8 bytes of the address space.

Sx specific differences:
0x02: Port C data (RW) [top 6 bits do nothing (read as 1s) on Sx parts]
0x06: Port C DDR (Write only, reads as 0xFF) [top 6 bits do nothing on Sx parts]
0x0B: Timer 2 Data Register MSB
0x0C: Timer 2 Data Register LSB
0x0D: Timer 2 Control Register
0x10: SPI Data Register
0x11: SPI Control Register
0x12-0x3F: Unused (reads as 0xFF?)

Port A has internal pull-ups (about 10kΩ), and can sink 1.6mA at 0.4V
making it capable of driving CMOS or one TTL load directly.

Port B has a true high-Z state (maximum 20µA for Px parts or 10µA for
Rx/Ux parts), and can sink 10mA at 1V (LED drive) or 3.2mA at 0.4V (two
TTL loads).  It requires external pull-ups to drive CMOS.

Port C has a true high-Z state (maximum 20µA for Px parts or 10µA for
Rx/Ux parts), and can sink 1.6mA at 0.4V (one TTL load).  It requires
external pull-ups to drive CMOS.

MOR ADDRESS: Mask Option Register; does not exist on R2 and several other but not all mask parts, located at 0x784 on Px parts

Px Parts:
* 28 pins
* address space is 0x000-0x7ff
* has Ports A-C
* port C is just 4 bits
* EPROM parts have MOR at 0x784 and bootstrap ROM at 0x785-0x7f7
* mask parts have a selftest rom at similar area

Rx Parts:
* 40 pins
* address space is 0x000-0xfff with an unused hole at 0xf39-0xf7f
* R2 parts have an unused hole at 0x10-0x3f and at 0x100-0x7bF
* has A/D converter, Ports A-D
* mask parts lack programmable prescaler
* EPROM parts have MOR at 0xf38 and bootstrap ROM at 0xf80-0xff7
* mask parts have selftest ROM at 0xf38-0xff7
* selftest ROMs differ between the U2 and U3 versions

Sx Parts:
* 40 pins
* address space is 0x000-0xfff with an unused hole at 0x12-0x3f and at 0x100-0x9BF
* has A/D converter, SPI serial
* port C is just two bits
* has an extra 16-bit timer compared to Ux/Rx
* selftest rom at 0xF00-0xFF7

Ux Parts:
* 40 pins
* address space is 0x000-0xfff with an unused hole at 0xf39-0xf7f
* U2 parts have an unused hole at 0x10-0x3f and at 0x100-0x7bF
* has Ports A-D
* EPROM parts have MOR at 0xf38 and bootstrap ROM at 0xf80-0xff7
* mask parts have selftest ROM at 0xf38-0xff7
* selftest ROMs differ between the U2 and U3 versions

*/

m68705_new_device::m68705_new_device(
		machine_config const &mconfig,
		char const *tag,
		device_t *owner,
		u32 clock,
		device_type type,
		char const *name,
		u32 addr_width,
		address_map_delegate internal_map,
		char const *shortname,
		char const *source)
	: m68705_device(mconfig, tag, owner, clock, type, name, addr_width, internal_map, shortname, source)
	, device_nvram_interface(mconfig, *this)
	, m_user_rom(*this, DEVICE_SELF, u32(1) << addr_width)
	, m_port_open_drain{ false, false, false, false }
	, m_port_mask{ 0x00, 0x00, 0x00, 0x00 }
	, m_port_input{ 0xff, 0xff, 0xff, 0xff }
	, m_port_latch{ 0xff, 0xff, 0xff, 0xff }
	, m_port_ddr{ 0x00, 0x00, 0x00, 0x00 }
	, m_port_cb_r{ { *this }, { *this }, { *this }, { *this } }
	, m_port_cb_w{ { *this }, { *this }, { *this }, { *this } }
	, m_prescaler(0x7f)
	, m_tdr(0xff)
	, m_tcr(0x7f)
	, m_vihtp(CLEAR_LINE)
	, m_pcr(0xff)
	, m_pl_data(0xff)
	, m_pl_addr(0xffff)
{
}

template <offs_t B> READ8_MEMBER(m68705_new_device::eprom_r)
{
	// read locked out when /VPON and /PLE are asserted
	return (!pcr_vpon() || !pcr_ple()) ? m_user_rom[B + offset] : 0xff;
}

template <offs_t B> WRITE8_MEMBER(m68705_new_device::eprom_w)
{
	// programming latch enabled when /VPON and /PLE are asserted
	if (pcr_vpon() && pcr_ple())
	{
		if (!pcr_pge())
		{
			m_pl_data = data;
			m_pl_addr = B + offset;
		}
		else
		{
			// this causes undefined behaviour, which is bad when EPROM programming is involved
			logerror("warning: write to EPROM when /PGE = 0 (%x = %x)\n", B + offset, data);
		}
	}
}

template <std::size_t N> void m68705_new_device::set_port_open_drain(bool value)
{
	m_port_open_drain[N] = value;
}

template <std::size_t N> void m68705_new_device::set_port_mask(u8 mask)
{
	m_port_mask[N] = mask;
}

template <std::size_t N> READ8_MEMBER(m68705_new_device::port_r)
{
	if (!m_port_cb_r[N].isnull()) m_port_input[N] = m_port_cb_r[N](space, 0, ~m_port_ddr[N]);
	return m_port_mask[N] | (m_port_latch[N] & m_port_ddr[N]) | (m_port_input[N] & ~m_port_ddr[N]);
}

template <std::size_t N> WRITE8_MEMBER(m68705_new_device::port_latch_w)
{
	data &= ~m_port_mask[N];
	u8 const diff = m_port_latch[N] ^ data;
	m_port_latch[N] = data;
	if (diff & m_port_ddr[N])
		port_cb_w<N>();
}

template <std::size_t N> WRITE8_MEMBER(m68705_new_device::port_ddr_w)
{
	data &= ~m_port_mask[N];
	if (data != m_port_ddr[N])
	{
		m_port_ddr[N] = data;
		port_cb_w<N>();
	}
}

template <std::size_t N> void m68705_new_device::port_cb_w()
{
	u8 const data(m_port_open_drain[N] ? m_port_latch[N] | ~m_port_ddr[N] : m_port_latch[N]);
	u8 const mask(m_port_open_drain[N] ? (~m_port_latch[N] & m_port_ddr[N]) : m_port_ddr[N]);
	m_port_cb_w[N](space(AS_PROGRAM), 0, data, mask);
}

READ8_MEMBER(m68705_new_device::tdr_r)
{
	return m_tdr;
}

WRITE8_MEMBER(m68705_new_device::tdr_w)
{
	m_tdr = data;
}

READ8_MEMBER(m68705_new_device::tcr_r)
{
	// in MOR controlled mode, only TIR, TIM and TOPT are visible
	return m_tcr | (tcr_topt() ? 0x37 : 0x00);
}

WRITE8_MEMBER(m68705_new_device::tcr_w)
{
	// 7  TIR   RW  Timer Interrupt Request Status
	// 6  TIM   RW  Timer Interrupt Mask
	// 5  TIN   RW  Timer Input Select
	// 4  TIE   RW  Timer External Input Enable
	// 3  TOPT  R   Timer Mask/Programmable Option
	// 3  PSC    W  Prescaler Clear
	// 2  PS2   RW  Prescaler Option
	// 1  PS1   RW  Prescaler Option
	// 0  PS0   RW  Prescaler Option

	// TIN  TIE  CLOCK
	//  0    0   Internal Clock (phase 2)
	//  0    1   Gated (AND) of External and Internal Clocks
	//  1    0   No Clock
	//  1    1   External Clock

	// in MOR controlled mode, TIN/PS2/PS1/PS0 are loaded from MOR on reset and TIE is always 1
	// in MOR controlled mode, TIN, TIE, PS2, PS1, and PS0 always read as 1

	// TOPT isn't a real bit in this register, it's a pass-through to the MOR register
	// it's theoretically possible to get into a weird state by writing to the MOR while running
	// for simplicity, we don't emulate this - we just check the MOR on initialisation and reset

	if (tcr_topt())
	{
		m_tcr = (m_tcr & 0x3f) | (data & 0xc0);
	}
	else
	{
		if (BIT(data, 3))
			m_prescaler = 0;
		m_tcr = (m_tcr & 0x08) | (data & 0xf7);
	}

	// TODO: this is tied up with /INT2 on R/U devices
	if (tcr_tir() && !tcr_tim())
		set_input_line(M68705_INT_TIMER, ASSERT_LINE);
	else
		set_input_line(M68705_INT_TIMER, CLEAR_LINE);
}

READ8_MEMBER(m68705_new_device::misc_r)
{
	logerror("unsupported read MISC\n");
	return 0xff;
}

WRITE8_MEMBER(m68705_new_device::misc_w)
{
	logerror("unsupported write MISC = %02X\n", data);
}

READ8_MEMBER(m68705_new_device::pcr_r)
{
	return m_pcr;
}

WRITE8_MEMBER(m68705_new_device::pcr_w)
{
	// 7  1
	// 6  1
	// 5  1
	// 4  1
	// 3  1
	// 2  /VPON  R   Vpp On
	// 1  /PGE   RW  Program Enable
	// 0  /PLE   RW  Programming Latch Enable

	// lock out /PGE if /PLE is not asserted
	data |= ((data & 0x01) << 1);

	// write EPROM if /PGE is asserted (erase requires UV so don't clear bits)
	if (pcr_vpon() && !pcr_pge() && !BIT(data, 1))
		m_user_rom[m_pl_addr] |= m_pl_data;

	m_pcr = (m_pcr & 0xfc) | (data & 0x03);
}

READ8_MEMBER(m68705_new_device::acr_r)
{
	logerror("unsupported read ACR\n");
	return 0xff;
}

WRITE8_MEMBER(m68705_new_device::acr_w)
{
	// 7  conversion complete
	// 6
	// 5
	// 4
	// 3
	// 2  MUX
	// 1  MUX
	// 0  MUX

	// MUX=0  AN0 (PD0)
	// MUX=1  AN1 (PD1)
	// MUX=2  AN2 (PD2)
	// MUX=3  AN3 (PD3)
	// MUX=4  VRH (PD5)  $FF+0/-1
	// MUX=5  VRL (PD4)  $00+1/-0
	// MUX=6  VRH/4      $40+/-1
	// MUX=7  VRH/2      $80+/-1

	// on-board ratiometric successive approximation ADC
	// 30 machine cycle conversion time (input sampled during first 5 cycles)
	// on completion, ACR7 is set, result is placed in ARR, and new conversion starts
	// writing to ACR aborts current conversion, clears ACR7, and starts new conversion

	logerror("unsupported write ACR = %02X\n", data);
}

READ8_MEMBER(m68705_new_device::arr_r)
{
	logerror("unsupported read ARR\n");
	return 0xff;
}

WRITE8_MEMBER(m68705_new_device::arr_w)
{
	logerror("unsupported write ARR = %02X\n", data);
}

void m68705_new_device::device_start()
{
	m68705_device::device_start();

	save_item(NAME(m_port_input));
	save_item(NAME(m_port_latch));
	save_item(NAME(m_port_ddr));

	save_item(NAME(m_prescaler));
	save_item(NAME(m_tdr));
	save_item(NAME(m_tcr));

	save_item(NAME(m_vihtp));
	save_item(NAME(m_pcr));
	save_item(NAME(m_pl_data));
	save_item(NAME(m_pl_addr));

	// initialise digital I/O
	for (u8 &input : m_port_input) input = 0xff;
	for (devcb_read8 &cb : m_port_cb_r) cb.resolve();
	for (devcb_write8 &cb : m_port_cb_w) cb.resolve_safe();

	// initialise timer/counter
	u8 const options(get_mask_options());
	m_tcr = 0x40 | (options & 0x37);
	if (BIT(options, 6))
		m_tcr |= 0x18;

	// initialise EPROM control
	m_vihtp = CLEAR_LINE;
	m_pcr = 0xff;
	m_pl_data = 0xff;
	m_pl_addr = 0xffff;
}

void m68705_new_device::device_reset()
{
	m68705_device::device_reset();

	// reset digital I/O
	port_ddr_w<0>(space(AS_PROGRAM), 0, 0x00, 0xff);
	port_ddr_w<1>(space(AS_PROGRAM), 0, 0x00, 0xff);
	port_ddr_w<2>(space(AS_PROGRAM), 0, 0x00, 0xff);
	port_ddr_w<3>(space(AS_PROGRAM), 0, 0x00, 0xff);

	// reset timer/counter
	u8 const options(get_mask_options());
	m_prescaler = 0x7f;
	m_tdr = 0xff;
	m_tcr = BIT(options, 6) ? (0x58 | (options & 0x27)) : (0x40 | (m_tcr & 0x37));

	// reset EPROM control
	m_pcr |= 0xfb; // b2 (/VPON) is driven by external input and hence unaffected by reset

	if (CLEAR_LINE != m_vihtp)
		RM16(0xfff6, &m_pc);
}

void m68705_new_device::execute_set_input(int inputnum, int state)
{
	switch (inputnum)
	{
	case M68705_VPP_LINE:
		m_pcr = (m_pcr & 0xfb) | ((ASSERT_LINE == state) ? 0x00 : 0x04);
		break;
	case M68705_VIHTP_LINE:
		// TODO: this is actually the same physical pin as the timer input, so they should be tied up
		m_vihtp = (ASSERT_LINE == state) ? ASSERT_LINE : CLEAR_LINE;
		break;
	default:
		m68705_device::execute_set_input(inputnum, state);
	}
}

void m68705_new_device::nvram_default()
{
}

void m68705_new_device::nvram_read(emu_file &file)
{
	file.read(&m_user_rom[0], m_user_rom.bytes());
}

void m68705_new_device::nvram_write(emu_file &file)
{
	file.write(&m_user_rom[0], m_user_rom.bytes());
}

void m68705_new_device::burn_cycles(unsigned count)
{
	// handle internal timer/counter source
	if (!tcr_tin()) // TODO: check tcr_tie() and gate on TIMER if appropriate
	{
		unsigned const ps_opt(tcr_ps());
		unsigned const ps_mask((1 << ps_opt) - 1);
		unsigned const decrements((count + (m_prescaler & ps_mask)) >> ps_opt);

		if (decrements && (decrements >= m_tdr))
		{
			m_tcr |= 0x80;
			if (!tcr_tim())
				set_input_line(M68705_INT_TIMER, ASSERT_LINE);
		}
		m_prescaler = (count + m_prescaler) & 0x7f;
		m_tdr = (m_tdr - decrements) & 0xff;
	}
}


/****************************************************************************
 * M68705Px family
 ****************************************************************************/

DEVICE_ADDRESS_MAP_START( p_map, 8, m68705p_device )
	ADDRESS_MAP_GLOBAL_MASK(0x07ff)
	ADDRESS_MAP_UNMAP_HIGH

	AM_RANGE(0x0000, 0x0000) AM_READWRITE(port_r<0>, port_latch_w<0>)
	AM_RANGE(0x0001, 0x0001) AM_READWRITE(port_r<1>, port_latch_w<1>)
	AM_RANGE(0x0002, 0x0002) AM_READWRITE(port_r<2>, port_latch_w<2>)
	// 0x0003 not used (no port D)
	AM_RANGE(0x0004, 0x0004) AM_WRITE(port_ddr_w<0>)
	AM_RANGE(0x0005, 0x0005) AM_WRITE(port_ddr_w<1>)
	AM_RANGE(0x0006, 0x0006) AM_WRITE(port_ddr_w<2>)
	// 0x0007 not used (no port D)
	AM_RANGE(0x0008, 0x0008) AM_READWRITE(tdr_r, tdr_w)
	AM_RANGE(0x0009, 0x0009) AM_READWRITE(tcr_r, tcr_w)
	// 0x000a not used
	AM_RANGE(0x000b, 0x000b) AM_READWRITE(pcr_r, pcr_w)
	// 0x000c-0x000f not used
	AM_RANGE(0x0010, 0x007f) AM_RAM
	AM_RANGE(0x0080, 0x0784) AM_READWRITE(eprom_r<0x0080>, eprom_w<0x0080>) // User EPROM
	AM_RANGE(0x0785, 0x07f7) AM_ROM AM_REGION("bootstrap", 0)
	AM_RANGE(0x07f8, 0x07ff) AM_READWRITE(eprom_r<0x07f8>, eprom_w<0x07f8>) // Interrupt vectors
ADDRESS_MAP_END

m68705p_device::m68705p_device(
		machine_config const &mconfig,
		char const *tag,
		device_t *owner,
		u32 clock,
		device_type type,
		char const *name,
		char const *shortname,
		char const *source)
	: m68705_new_device(mconfig, tag, owner, clock, type, name, 11, address_map_delegate(FUNC(m68705p_device::p_map), this), shortname, source)
{
	set_port_open_drain<0>(true);   // Port A is open drain with internal pull-ups
	set_port_mask<2>(0xf0);         // Port C is four bits wide
	set_port_mask<3>(0xff);         // Port D isn't present
}

offs_t m68705p_device::disasm_disassemble(
		std::ostream &stream,
		offs_t pc,
		const uint8_t *oprom,
		const uint8_t *opram,
		uint32_t options)
{
	return CPU_DISASSEMBLE_NAME(m6805)(this, stream, pc, oprom, opram, options, m68705p_syms);
}


/****************************************************************************
 * M68705Ux family
 ****************************************************************************/

DEVICE_ADDRESS_MAP_START( u_map, 8, m68705u_device )
	ADDRESS_MAP_GLOBAL_MASK(0x0fff)
	ADDRESS_MAP_UNMAP_HIGH

	AM_RANGE(0x0000, 0x0000) AM_READWRITE(port_r<0>, port_latch_w<0>)
	AM_RANGE(0x0001, 0x0001) AM_READWRITE(port_r<1>, port_latch_w<1>)
	AM_RANGE(0x0002, 0x0002) AM_READWRITE(port_r<2>, port_latch_w<2>)
	AM_RANGE(0x0003, 0x0003) AM_READWRITE(port_r<3>, port_latch_w<3>)
	AM_RANGE(0x0004, 0x0004) AM_WRITE(port_ddr_w<0>)
	AM_RANGE(0x0005, 0x0005) AM_WRITE(port_ddr_w<1>)
	AM_RANGE(0x0006, 0x0006) AM_WRITE(port_ddr_w<2>)
	// 0x0007 not used (port D is input only)
	AM_RANGE(0x0008, 0x0008) AM_READWRITE(tdr_r, tdr_w)
	AM_RANGE(0x0009, 0x0009) AM_READWRITE(tcr_r, tcr_w)
	AM_RANGE(0x000a, 0x000a) AM_READWRITE(misc_r, misc_w)
	AM_RANGE(0x000b, 0x000b) AM_READWRITE(pcr_r, pcr_w)
	// 0x000c-0x000f not used
	AM_RANGE(0x0010, 0x007f) AM_RAM
	AM_RANGE(0x0080, 0x0f38) AM_READWRITE(eprom_r<0x0080>, eprom_w<0x0080>) // User EPROM
	// 0x0f39-0x0f7f not used
	AM_RANGE(0x0f80, 0x0ff7) AM_ROM AM_REGION("bootstrap", 0)
	AM_RANGE(0x0ff8, 0x0fff) AM_READWRITE(eprom_r<0x0ff8>, eprom_w<0x0ff8>) // Interrupt vectors
ADDRESS_MAP_END

m68705u_device::m68705u_device(
		machine_config const &mconfig,
		char const *tag,
		device_t *owner,
		u32 clock,
		device_type type,
		char const *name,
		address_map_delegate internal_map,
		char const *shortname,
		char const *source)
	: m68705_new_device(mconfig, tag, owner, clock, type, name, 12, internal_map, shortname, source)
{
	set_port_open_drain<0>(true);   // Port A is open drain with internal pull-ups
}

m68705u_device::m68705u_device(
		machine_config const &mconfig,
		char const *tag,
		device_t *owner,
		u32 clock,
		device_type type,
		char const *name,
		char const *shortname,
		char const *source)
	: m68705u_device(mconfig, tag, owner, clock, type, name, address_map_delegate(FUNC(m68705u_device::u_map), this), shortname, source)
{
}

offs_t m68705u_device::disasm_disassemble(
		std::ostream &stream,
		offs_t pc,
		const uint8_t *oprom,
		const uint8_t *opram,
		uint32_t options)
{
	return CPU_DISASSEMBLE_NAME(m6805)(this, stream, pc, oprom, opram, options, m68705u_syms);
}


/****************************************************************************
 * M68705Rx family
 ****************************************************************************/

DEVICE_ADDRESS_MAP_START( r_map, 8, m68705r_device )
	ADDRESS_MAP_GLOBAL_MASK(0x0fff)
	ADDRESS_MAP_UNMAP_HIGH

	AM_RANGE(0x0000, 0x0000) AM_READWRITE(port_r<0>, port_latch_w<0>)
	AM_RANGE(0x0001, 0x0001) AM_READWRITE(port_r<1>, port_latch_w<1>)
	AM_RANGE(0x0002, 0x0002) AM_READWRITE(port_r<2>, port_latch_w<2>)
	AM_RANGE(0x0003, 0x0003) AM_READWRITE(port_r<3>, port_latch_w<3>)
	AM_RANGE(0x0004, 0x0004) AM_WRITE(port_ddr_w<0>)
	AM_RANGE(0x0005, 0x0005) AM_WRITE(port_ddr_w<1>)
	AM_RANGE(0x0006, 0x0006) AM_WRITE(port_ddr_w<2>)
	// 0x0007 not used (port D is input only)
	AM_RANGE(0x0008, 0x0008) AM_READWRITE(tdr_r, tdr_w)
	AM_RANGE(0x0009, 0x0009) AM_READWRITE(tcr_r, tcr_w)
	AM_RANGE(0x000a, 0x000a) AM_READWRITE(misc_r, misc_w)
	AM_RANGE(0x000b, 0x000b) AM_READWRITE(pcr_r, pcr_w)
	// 0x000c-0x000d not used
	AM_RANGE(0x000e, 0x000e) AM_READWRITE(acr_r, acr_w)
	AM_RANGE(0x000f, 0x000f) AM_READWRITE(arr_r, arr_w)
	AM_RANGE(0x0010, 0x007f) AM_RAM
	AM_RANGE(0x0080, 0x0f38) AM_READWRITE(eprom_r<0x0080>, eprom_w<0x0080>) // User EPROM
	// 0x0f39-0x0f7f not used
	AM_RANGE(0x0f80, 0x0ff7) AM_ROM AM_REGION("bootstrap", 0)
	AM_RANGE(0x0ff8, 0x0fff) AM_READWRITE(eprom_r<0x0ff8>, eprom_w<0x0ff8>) // Interrupt vectors
ADDRESS_MAP_END

m68705r_device::m68705r_device(
		machine_config const &mconfig,
		char const *tag,
		device_t *owner,
		u32 clock,
		device_type type,
		char const *name,
		char const *shortname,
		char const *source)
	: m68705u_device(mconfig, tag, owner, clock, type, name, address_map_delegate(FUNC(m68705r_device::r_map), this), shortname, source)
{
}

offs_t m68705r_device::disasm_disassemble(
		std::ostream &stream,
		offs_t pc,
		const uint8_t *oprom,
		const uint8_t *opram,
		uint32_t options)
{
	return CPU_DISASSEMBLE_NAME(m6805)(this, stream, pc, oprom, opram, options, m68705r_syms);
}


/****************************************************************************
 * M68705P3 device
 ****************************************************************************/

m68705p3_device::m68705p3_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock)
	: m68705p_device(mconfig, tag, owner, clock, M68705P3, "MC68705P3", "m68705p3", __FILE__)
{
}

tiny_rom_entry const *m68705p3_device::device_rom_region() const
{
	return ROM_NAME(m68705p3);
}

u8 m68705p3_device::get_mask_options() const
{
	return get_user_rom()[0x0784] & 0xf7; // no SNM bit
}


/****************************************************************************
 * M68705P5 device
 ****************************************************************************/

m68705p5_device::m68705p5_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock)
	: m68705p_device(mconfig, tag, owner, clock, M68705P3, "MC68705P5", "m68705p5", __FILE__)
{
}

tiny_rom_entry const *m68705p5_device::device_rom_region() const
{
	return ROM_NAME(m68705p5);
}

u8 m68705p5_device::get_mask_options() const
{
	return get_user_rom()[0x0784];
}


/****************************************************************************
 * M68705R3 device
 ****************************************************************************/

m68705r3_device::m68705r3_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock)
	: m68705r_device(mconfig, tag, owner, clock, M68705R3, "MC68705R3", "m68705r3", __FILE__)
{
}

tiny_rom_entry const *m68705r3_device::device_rom_region() const
{
	return ROM_NAME(m68705r3);
}

u8 m68705r3_device::get_mask_options() const
{
	return get_user_rom()[0x0784] & 0xf7; // no SNM bit
}


/****************************************************************************
 * M68705U3 device
 ****************************************************************************/

m68705u3_device::m68705u3_device(machine_config const &mconfig, char const *tag, device_t *owner, uint32_t clock)
	: m68705u_device(mconfig, tag, owner, clock, M68705U3, "MC68705U3", "m68705u3", __FILE__)
{
}

tiny_rom_entry const *m68705u3_device::device_rom_region() const
{
	return ROM_NAME(m68705u3);
}

u8 m68705u3_device::get_mask_options() const
{
	return get_user_rom()[0x0784] & 0xf7; // no SNM bit
}
