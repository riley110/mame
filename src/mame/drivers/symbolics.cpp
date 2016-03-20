// license:BSD-3-Clause
// copyright-holders:Jonathan Gevaryahu
/******************************************************************************
	Symbolics 36x0 (really in this case, 3670; the original 3600 is considerably rarer, 3670 is backwards compatible for the most part)
	TODO: add credits, backstory, history, etc here
	Front-end Processor dumped only, so far, plds/proms/pals not dumped yet
	
	TODO:
	The entire lispcpu half (more like 3/4) of the machine
	Framebuffer 1152x864? (lives on the i/o card)
	I8274 MPSC (z80dart.cpp) x2
	1024x4bit SRAM AM2148-50 x6 @F22-F27
	2048x8bit SRAM @F7 and @G7
	keyboard/mouse (a 68k based console dedicated to this machine; talks through one of the MPSC chips)
	am9517a-50 DMA controller
	'NanoFEP' i8749 mcu which runs the front panel and rtc clock (only on 3600, the 3670 lacks this)
	
	DONE:
	ROM Loading
	256K DRAM
	
	Keyboard serial bit map:
	Local (#x01), Caps Lock (LED) (#x02), Hyper (left) (#x03), Meta (left) (#x04), Control (right) (#x05), Super (right) (#x06), Scroll (#x07), Mode Lock (LED) (#x08), Select (#x0c), Symbol (left) (#x0d), Super (left) (#x0e), Control (left) (#x0f), Space (#x10), Meta (right) (#x11), Hyper (right) (#x12), End (#x13), Z (#x17), C (#x18), B (#x19), M (#x1a), . / > (#x1b), Shift (right) (#x1c), Repeat (#x1d), Abort (#x1e), Shift (left) (#x22), X (#x23), V (#x24), N (#x25), , / < (#x26), / / ? (#x27), Symbol (right) (#x28), Help (#x29), Rubout (#x2d), S (#x2e), F (#x2f), H (#x30), K (#x31), ; / : (#x32), Return (#x33), Complete (#x34), Network (#x38), A (#x39), D (#x3a), G (#x3b), J (#x3c), L (#x3d), ' / " (#x3e), Line (#x3f), Function (#x43), W (#x44), R (#x45), Y (#x46), I (#x47), P (#x48), ) / ] (#x49), Page (#x4a), Tab (#x4e), Q (#x4f), E (#x50), T (#x51), U (#x52), O (#x53), ( / [ (#x54), Back Space (#x55), : (#x59), 2 / @ (#x5a), 4 / $ (#x5b), 6 / ^ (#x5c), 8 / * (#x5d), 0 / ) (#x5e), = / + (#x5f), \ / { (#x60), 1 / ! (#x64), 3 / # (#x65), 5 / % (#x66), 7 / & (#x67), 9 / ( (#x68), - / _ (#x69), ` / ~ (#x6a), | / } (#x6b), Escape (#x6f), Refresh (#x70), Square (#x71), Circle (#x72), Triangle (#x73), Clear Input (#x74), Suspend (#x75), Resume (#x76)
	
	
	Notes from US Patent 4887235 which has some FEP source code and limited memory info:
	FEP main description starts on pdf page 47
	FEP access the SPY bus which allows poking at the workings of the lisp cpu, as well as reading the ethernet interface (on 3600, later g-machine 3650 fep seems to use a separate z80 for this?) see patent pdf page 33
	page 41 reveals the SPY bus is 8 data bits wide, and has an address of 6 bits
	see page 97 of http://bitsavers.trailing-edge.com/pdf/symbolics/3600_series/Lisp_Machine_Hardware_Memos.pdf which explains what addresses do what.
	FEP accesses a register called SQCLKC described with bits on pdf page 32, figure out where this maps
	
	http://bitsavers.trailing-edge.com/pdf/symbolics/3600_series/3600_TechnicalSummary_Feb83.pdf <- page 114 describes the nanofep
	http://bitsavers.trailing-edge.com/pdf/symbolics/3600_series/Symbolics_3600_Series_Basic_Field_Maint_Jul86.pdf <- page 84 describes the two types of FEP, older one with firmware v24 and newer one (emulated here) with firmware v127
	
	fonts:
	tiny7: 0x908 to 0xB08 in fep ROM is a thin font, 4 or 5 pixels wide, rows are in a weird scrambled order
	normal or cptfont: 0x11e8 to 0x16d8 is a thick/wide font, 10? pixels wide
	verylarge: 0x2a48 to 0x3638 is an even wider font, 14 pixels wide
	tvfont?: 0x134d8-0x13ad0 is a thin font about 7 pixels wide with the rows in a weird scrambled order
******************************************************************************/

/* Core includes */
#include "emu.h"
#include "cpu/m68000/m68000.h"

class symbolics_state : public driver_device
{
public:

	symbolics_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu")
	{
	}

	required_device<m68000_base_device> m_maincpu;
	DECLARE_DRIVER_INIT(symbolics);
	DECLARE_READ16_MEMBER(buserror_r);

//protected:
//	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;
};

READ16_MEMBER(symbolics_state::buserror_r)
{
	if(!space.debugger_access())
	{
		m_maincpu->set_input_line(M68K_LINE_BUSERROR, ASSERT_LINE);
		m_maincpu->set_input_line(M68K_LINE_BUSERROR, CLEAR_LINE);
	}
	return 0;
}


/******************************************************************************
 Address Maps
******************************************************************************/
/*
Address maps (x = ignored; * = selects address within this range, ? = unknown, 1/0 = decodes only when this bit is set to 1/0)
68k address map:
a23 a22 a21 a20 a19 a18 a17 a16 a15 a14 a13 a12 a11 a10 a9  a8  a7  a6  a5  a4  a3  a2  a1  (a0 via UDS/LDS)
?   ?   ?   ?   ?   0   0   0   0   0   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   ROM 00H @D13 first half
?   ?   ?   ?   ?   0   0   0   0   0   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   ROM 00L @D7 first half
?   ?   ?   ?   ?   0   0   0   0   1   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   ROM 04H @D14 first half
?   ?   ?   ?   ?   0   0   0   0   1   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   ROM 04L @D8 first half
?   ?   ?   ?   ?   0   0   0   1   0   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   ROM 10H @D16 first half
?   ?   ?   ?   ?   0   0   0   1   0   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   ROM 10L @D10 first half
?   ?   ?   ?   ?   0   0   0   1   1   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   Open Bus (socket @D17 first half)
?   ?   ?   ?   ?   0   0   0   1   1   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   Open Bus (socket @D11 first half)
?   ?   ?   ?   ?   0   0   1   0   0   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   ROM 00H @D13 second half
?   ?   ?   ?   ?   0   0   1   0   0   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   ROM 00L @D7 second half
?   ?   ?   ?   ?   0   0   1   0   1   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   ROM 04H @D14 second half
?   ?   ?   ?   ?   0   0   1   0   1   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   ROM 04L @D8 second half
?   ?   ?   ?   ?   0   0   1   1   0   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   ROM 10H @D16 second half
?   ?   ?   ?   ?   0   0   1   1   0   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   ROM 10L @D10 second half
?   ?   ?   ?   ?   0   0   1   1   1   *   *   *   *   *   *   *   *   *   *   *   *   *   0       R   Open Bus (socket @D17 second half)
?   ?   ?   ?   ?   0   0   1   1   1   *   *   *   *   *   *   *   *   *   *   *   *   *   1       R   Open Bus (socket @D11 second half)
?   ?   ?   ?   ?   0   1   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *       RW  RAM (4164 DRAMs; these have parity as well, which is checked in the i/o area somehow?)
1   1   1   1   1   1   1   1   ?   ?   ?   ?   ?   ?   ?   ?   ?   ?   ?   ?   ?   ?   ?   *       ?   SPY BUS and FEP peripherals for OLD FEP hardware, not the NFEP we have here; the map of this area is listed in octal on pdf page 399 of us patent 4887235. The NFEP map is certainly not the same, but is probably similar.
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   0   *   *   *   *       RW  SPY-CMEM (writes CMEM WD, reads UIR)
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   0   1   1   1   *       W   SPY-SQ-CTL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   0   1   1   1   0       R   SPY-BOARD-ID (read a given board's id prom, board select is in U AMRA register)
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   1   0   0   0   *       R   SPY-SQ-STATUS
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   1   0   0   1   *       ?   SPY-NEXT-CPC
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   1   0   0   1   0?      ?   SPY-TASK
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   1   0   0   1   1       R   SPY-CTOS-HIGH
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   0   1   0   1   1   0?      R   SPY-OPC
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   0   0   0   0?      W   SPY-MC-CONTROL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   0   0   0   0       R   SPY-MC-ID
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   0   0   0   1       R   SPY-MC-ERROR-STATUS
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   0   0   1   0       R   SPY-ECC-SYNDROME
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   0   0   1   1       R   SPY-ECC-ADDRESS
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   0   1   0   *?      R   SPY-MC-STATUS
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   1   0   0   0       W   SPY-NET-SELECT
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   0   1   1   0   1   0   0   1       W   SPY-NET-CONTROL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   0   0   0   0   0   x?  0       RW  MPSC-0-A
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   0   0   0   0   0   x?  1       RW  MPSC-0-B
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   0   0   0   0   1   x?  0       RW  MPSC-1-A
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   0   0   0   0   1   x?  1       RW  MPSC-1-B
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   0   0   0   1   0   1   *?      R?  SPY-DMA-HIGH-ADDRS
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   0   0   1   0   0   0   *?      R   SPY-DMA-CONTROLLER
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   0   1   0   0   0   0   ??      R   FEP-PADDLE-ID-PROM
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   0   0   0   0   0   ??      R   FEP-BOARD-ID-PROM
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   0   0   0   0   1       W   FEP-BOARD-ID-CONTROL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   0   0   0   1   1       W   FEP-DMA-AND-CLOCK-CTL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   0   0   1   0   0       W   FEP-DMA-CONTROL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   0   0   1   0   1       W   FEP-PROC-CONTROL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   0   1   0   0   0?      W   FEP(SPY)-LBUS-CONTROL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   0   1   1   0   0?      W   FEP-SERIAL-BAUD-RATE-0
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   0   1   1   1   0?      W   FEP-SERIAL-BAUD-RATE-1
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   1   0   0   0   0?      W   FEP-HSB-CONTROL
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   1   0   0   1   0?      W   FEP-HSB-DATA
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   1   0   1   0   0?      W   FEP-HSB-POINTER
1   1   1   1   1   1   1   1   0   0   0   0   0   0   0   0   1   1   1   1   1   0   0   0?      W   P-PORT
              |               |               |               |               |               |     hex
          |           |           |           |           |           |           |           |     octal
@310: write 
@A2A4: write to FF  
 

*/

static ADDRESS_MAP_START(m68k_mem, AS_PROGRAM, 16, symbolics_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x000000, 0x01ffff) AM_ROM /* ROM lives here, and writing to 0x00-0x08 writes to the main lisp ram? */
	//AM_RANGE(0x020000, 0x03ffff) AM_RAM /* Local FEP ram seems to be here? there are 18 mcm4164s on the pcb which probably map here, plus 2 parity bits? */
	AM_RANGE(0x020000, 0x03ffff) AM_RAM /* Local FEP ram seems to be here? there are 18 mcm4164s on the pcb which probably map here, plus 2 parity bits? */
	// 2x AM9128-10PC 2048x8 SRAMs @F7 and @G7 map somewhere
	// 6x AM2148-50 1024x4bit SRAMs @F22-F27 map somewhere
	//AM_RANGE(0x040000, 0xffffff) AM_READ(buserror_r);
	//AM_RANGE(0x800000, 0xffffff) AM_RAM /* paged access to lispm ram? */
	//FF00B0 is readable, may be to read the MC/SQ/DP/AU continuity lines?
	//FF00E1 is writable, may control the LBUS_POWER_RESET line, see http://bitsavers.trailing-edge.com/pdf/symbolics/3600_series/Lisp_Machine_Hardware_Memos.pdf page 90
	//FF018A is writable, gets 0x5555 written to it
ADDRESS_MAP_END

static ADDRESS_MAP_START(m68k_io, AS_IO, 16, symbolics_state )
ADDRESS_MAP_END

/******************************************************************************
 Input Ports
******************************************************************************/
static INPUT_PORTS_START( symbolics )
INPUT_PORTS_END

/******************************************************************************
 Machine Drivers
******************************************************************************/
/*void symbolics_state::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	switch (id)
	{
	case TIMER_OUTFIFO_READ:
		outfifo_read_cb(ptr, param);
		break;
	default:
		assert_always(FALSE, "Unknown id in symbolics_state::device_timer");
	}
}

TIMER_CALLBACK_MEMBER(symbolics_state::outfifo_read_cb)
{
	UINT16 data;
}
*/

/* Driver init: stuff that needs setting up which isn't directly affected by reset */
DRIVER_INIT_MEMBER(symbolics_state,symbolics)
{
}

static MACHINE_CONFIG_START( symbolics, symbolics_state )
	/* basic machine hardware */
	// per page 159 of http://bitsavers.trailing-edge.com/pdf/symbolics/3600_series/Lisp_Machine_Hardware_Memos.pdf:
	//XTALS: 16MHz @H11 (68k CPU clock)
	//       4.9152MHz @J5 (driving the two MPSCs serial clocks)
	//       66.67MHz @J10 (main lispcpu/system clock)
	MCFG_CPU_ADD("maincpu", M68000, XTAL_16MHz/2) /* MC68000L8 @A27; clock is derived from the 16Mhz xtal @ H11 */
	MCFG_CPU_PROGRAM_MAP(m68k_mem)
	MCFG_CPU_IO_MAP(m68k_io)

	//ADD ME:
	// Framebuffer
	// DMA Controller
	// I8274 MPSC #1 (synchronous serial for keyboard)
	// I8274 MPSC #2 (EIA/debug console?)

MACHINE_CONFIG_END

/******************************************************************************
 ROM Definitions
******************************************************************************/

ROM_START( s3670 )
	ROM_REGION16_BE(0x40000,"maincpu", 0)
	// the older 'FEP V24' has similar roms but a different hw layout and memory map
	ROM_SYSTEM_BIOS( 0, "v127", "Symbolics 3600 L-Machine 'NFEP V127'")
	ROMX_LOAD("00h.127.27c128.d13", 0x00000, 0x2000, CRC(b8d7c8da) SHA1(663a09359f5db63beeac00e5c2783ccc25b94250), ROM_SKIP(1) | ROM_BIOS(1)) // Label: "00H.127" @D13
	ROM_CONTINUE( 0x10000, 0x2000 )
	ROMX_LOAD("00l.127.27128.d7", 0x00001, 0x2000, CRC(cc7bae9a) SHA1(057538eb821c4d00dde19cfe5136ccc0aee43800), ROM_SKIP(1) | ROM_BIOS(1)) // Label: "00L.127" @D7
	ROM_CONTINUE( 0x10001, 0x2000 )
	ROMX_LOAD("04h.127.27128.d14", 0x04000, 0x2000, CRC(e01a717b) SHA1(b87a670f7be13553485ce88fad5fcf90f01473c4), ROM_SKIP(1) | ROM_BIOS(1)) // Label: "04H.127" @D14
	ROM_CONTINUE( 0x14000, 0x2000 )
	ROMX_LOAD("04l.127.27128.d8", 0x04001, 0x2000, CRC(68d169fa) SHA1(d6fab3132fca332a9bedb174fddf5fc8c69d05b6), ROM_SKIP(1) | ROM_BIOS(1)) // Label: "04L.127" @D8
	ROM_CONTINUE( 0x14001, 0x2000 )
	ROMX_LOAD("10h.127.27128.d16", 0x08000, 0x2000, CRC(2ea7a70d) SHA1(61cc97aada028612c24d788d946d77e82116cf30), ROM_SKIP(1) | ROM_BIOS(1)) // Label: "10H.127" @D16
	ROM_CONTINUE( 0x18000, 0x2000 )
	ROMX_LOAD("10l.127.27c128.d10", 0x08001, 0x2000, CRC(b8ddb3c8) SHA1(e6c3b96340c5c767ef18abf48b73fa8e5d7353b9), ROM_SKIP(1) | ROM_BIOS(1)) // Label: "10L.127" @D10
	ROM_CONTINUE( 0x18001, 0x2000 )
	// D17, D11 are empty sockets; these would map to 0x0c000-0ffff and 0x1c000-0x1ffff
	// note: load all the PLAs, PALs and PROMs here
	// picture is at https://4310b1a9-a-11c96037-s-sites.googlegroups.com/a/ricomputermuseum.org/home/Home/equipment/symbolics-3645/Symbolics_3645_FEP.jpg
	/*
		LBBUFA.4 mb7124   @A6
		LBBUFB.4 mb7124   @A7
		LBBUFC.4 mb7124   @A9
		LBAAR.4           @A12
		LBPAR.4A          @A13
		PROCA.4  pal16R8A @A25
		HSADR.4  pal1???? @C4
		DYNMEM.5 pal16R8A @C20
		PCDYNCTL          @C21
		REQSEL.4A         @C22
		DY2ACK   pal16L8A @C23
		PROC.4   pal?     @C25
		UDMAHA.4 pal?     @D3
		FEP 4642 16pprom? @D5 <- this is the serial number of the FEP board stored in a prom, readable at one of the local-io addresses
		HRSQ.4   pal      @D6
		d7, d8, d10 are eproms, see above
		d11 is empty socket marked 2764
		d13, d14, d16 are eproms, see above
		d17 is empty socket marked 2764
		?DVZNUR?          @E21 <- unreadable label, recheck!
		LDBD.4   pal16L8A @G18
		PAGTAG.5          @H20
		UDMABC.4 pal      @I4
		SERDMA.4          @I8
		SERIAB.4          @I9
		LBARB.4           @I18
		SERCTL.4          @K6
	*/
ROM_END

/******************************************************************************
 Drivers
******************************************************************************/

/*    YEAR  NAME        PARENT  COMPAT  MACHINE     INPUT       STATE            INIT        COMPANY      FULLNAME  FLAGS */
COMP( 1984, s3670,      0,      0,      symbolics,  symbolics,  symbolics_state, symbolics,  "Symbolics", "3670",   MACHINE_IS_SKELETON | MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
