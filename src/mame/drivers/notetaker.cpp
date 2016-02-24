// license:BSD-3-Clause
// copyright-holders:Jonathan Gevaryahu
/* Xerox NoteTaker, 1978
 * Driver by Jonathan Gevaryahu

 *  Notetaker Team At Xerox PARC 1976-1980:
     Alan Kay - Team Lead
     Bruce Horn - BIOS code and more
     Ted Kaehler - SmallTalk-76 code porting[2] and more ( http://tedkaehler.weather-dimensions.com/us/ted/index.html )
     Dan Ingalls - BitBlt engine and SmallTalk kernel and more[3]
     Doug Fairbairn - NoteTaker Hardware/Electronics Design ( http://www.computerhistory.org/atchm/author/dfairbairn/ )
     James Leung - NoteTaker Hardware/Electronics Design
     Ron Freeman - NoteTaker Hardware/Electronics Design
     <there are probably others I've missed>

 * History of the machine can be found at http://freudenbergs.de/bert/publications/Ingalls-2014-Smalltalk78.pdf

 * Prototypes only, 10 units[2] manufactured 1978-1980
   Known surviving units:
   * One at CHM (missing? mouse, no media)
   * One at Xerox Museum at PARC (with mouse and 2 floppies, floppies were not imaged to the best of my knowledge)

 * The NoteTaker used the BitBlt graphical operation (from SmallTalk-76) to do most graphical functions, in order to fit the SmallTalk code and programs within 256K of RAM[2]. The actual BitBlt code lives in ROM[3].

 * As far as I am aware, no media (world disks/boot disks) for the NoteTaker have survived (except maybe the two disks at Xerox Museum at PARC), but an incomplete dump of the Smalltalk-76 'world' which was used to bootstrap Smalltalk-78 originally did survive on the Alto disks at CHM

 * see http://bitsavers.informatik.uni-stuttgart.de/pdf/xerox/notetaker for additional information
 * see http://xeroxalto.computerhistory.org/Filene/Smalltalk-76/ for the smalltalk-76 dump
 * see http://xeroxalto.computerhistory.org/Indigo/BasicDisks/Smalltalk14.bfs!1_/ for more notetaker/smalltalk related files, including SmallTalk-80 files based on the notetaker smalltalk-78

 References:
 * [1] http://freudenbergs.de/bert/publications/Ingalls-2014-Smalltalk78.pdf
 * [2] "Smalltalk and Object Orientation: An Introduction" By John Hunt, pages 45-46 [ISBN 978-3-540-76115-0]
 * [3] http://bitsavers.trailing-edge.com/pdf/xerox/notetaker/memos/19790620_Z-IOP_1.5_ls.pdf
 * [4] http://xeroxalto.computerhistory.org/Filene/Smalltalk-76/
 * [5] http://bitsavers.trailing-edge.com/pdf/xerox/notetaker/memos/19790118_NoteTaker_System_Manual.pdf
 * MISSING DUMP for 8741? Keyboard MCU which does row-column scanning and mouse-related stuff

TODO: everything below.
* figure out the correct memory maps for the 256kB of shared ram, and what part of ram constitutes the framebuffer
* figure out how the emulation-cpu boots and where its 4k of local ram maps to
* Get smalltalk-78 loaded as a rom and forced into ram on startup, since no boot disks have survived (or if any survived, they are not dumped)
* crt5027 video controller
* Harris 6402 keyboard UART (within keyboard, next to MCU)
* The harris 6402 UART is pin compatible with WD TR1865 and TR1602 UARTs, as well as the AY-3-1015A/D
* HLE for the missing i8748[5] MCU in the keyboard which reads the mouse quadratures and buttons and talks serially to the Keyboard UART

WIP:
* pic8259 interrupt controller - this is attached as a device, but the interrupts are not hooked to it yet.
* Harris 6402 serial/EIA UART
* Harris 6402 keyboard UART (within notetaker)
* floppy controller wd1791
  According to [3] and [5] the format is double density/MFM, 128 bytes per sector, 16 sectors per track, 1 or 2 sided, for 170K or 340K per disk. Drive spins at 300RPM.
  According to the schematics, we're missing an 82s147 DISKSEP.PROM used as a data separator
* we're missing a dump of the 82s126 SETMEMRQ PROM used to handle memory arbitration between the crtc and the rest of the system, but the equations are on the schematic and I'm planning to regenerate the prom contents from that, see ROM_LOAD section

DONE:
* i/o cpu i/o area needs the memory map worked out per the schematics - done
*/

#include "cpu/i86/i86.h"
#include "machine/pic8259.h"
#include "machine/ay31015.h"
#include "video/tms9927.h"

class notetaker_state : public driver_device
{
public:
	notetaker_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag) ,
		m_iocpu(*this, "iocpu"),
		m_iopic(*this, "iopic8259"),
		m_kbduart(*this, "kbduart"),
		m_eiauart(*this, "eiauart"),
		m_crtc(*this, "crt5027")
	{
	}
// devices
	required_device<cpu_device> m_iocpu;
	required_device<pic8259_device> m_iopic;
	required_device<ay31015_device> m_kbduart;
	required_device<ay31015_device> m_eiauart;
	required_device<crt5027_device> m_crtc;

//declarations
	// screen
	UINT32 screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	// basic io
	DECLARE_WRITE16_MEMBER(IPConReg_w);

	// uarts
	DECLARE_READ16_MEMBER(ReadKeyData_r);
	DECLARE_READ16_MEMBER(ReadOPStatus_r);
	DECLARE_WRITE16_MEMBER(LoadKeyData_w);
	DECLARE_WRITE16_MEMBER(LoadKeyCtlReg_w);
	DECLARE_WRITE16_MEMBER(KeyDataReset_w);
	DECLARE_WRITE16_MEMBER(KeyChipReset_w);
	DECLARE_READ16_MEMBER(ReadEIAData_r);
	DECLARE_READ16_MEMBER(ReadEIAStatus_r);
	DECLARE_WRITE16_MEMBER(LoadEIAData_w);
	DECLARE_WRITE16_MEMBER(LoadEIACtlReg_w);
	DECLARE_WRITE16_MEMBER(EIADataReset_w);
	DECLARE_WRITE16_MEMBER(EIAChipReset_w);
	// mem map stuff
	DECLARE_READ16_MEMBER(iocpu_r);
	DECLARE_WRITE16_MEMBER(iocpu_w);
	DECLARE_DRIVER_INIT(notetakr);
//variables
	UINT8 m_BootSeqDone;
	UINT8 m_DisableROM;

// overrides
	virtual void machine_reset() override;
};

UINT32 notetaker_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	// have to figure out what resolution we're drawing to here and draw appropriately to screen
	return 0;
}

WRITE16_MEMBER(notetaker_state::IPConReg_w)
{
	m_BootSeqDone = (data&0x80)?1:0;
	//m_ProcLock = (data&0x40)?1:0; // processor lock
	//m_CharCtr = (data&0x20)?1:0; // battery charge control
	m_DisableROM = (data&0x10)?1:0; // disable rom at 0000-0fff
	//m_CorrOn = (data&0x08)?1:0; // also LedInd5
	//m_LedInd6 = (data&0x04)?1:0;
	//m_LedInd7 = (data&0x02)?1:0;
	//m_LedInd8 = (data&0x01)?1:0;
	popmessage("LEDS: CR1: %d, CR2: %d, CR3: %d, CR4: %d", (data&0x04)>>2, (data&0x08)>>3, (data&0x02)>>1, (data&0x01)); // cr1 and 2 are in the reverse order as expected, according to the schematic
}

/* handlers for the two system hd6402s (ay-5-1013 equivalent) */
/* * Keyboard hd6402 */
READ16_MEMBER( notetaker_state::ReadKeyData_r )
{
	return 0xFF00||m_kbduart->get_received_data();
}

READ16_MEMBER( notetaker_state::ReadOPStatus_r )
{
	UINT16 data = 0xFFF0;
	// TODO: FIX HACK: set FIFOInRDY and FIFOOutRdy both to 1
	data |= 0x0C;
	/*
	data |= (m_FIFOOutRdy) ? 0x08 : 0;
	data |= (m_FIFOInRdy) ? 0x04 : 0;
	*/
	// note /SWE is permanently enabled, so we don't enable it here for HD6402 reading
	data |= m_kbduart->get_output_pin(AY31015_DAV ) ? 0x02 : 0; // DR - pin 19
	data |= m_kbduart->get_output_pin(AY31015_TBMT) ? 0x01 : 0; // TBRE - pin 22
	return data;
}

WRITE16_MEMBER( notetaker_state::LoadKeyData_w )
{
	m_kbduart->set_transmit_data(data&0xFF);
}

WRITE16_MEMBER( notetaker_state::LoadKeyCtlReg_w )
{
	m_kbduart->set_input_pin(AY31015_CS, 0);
	m_kbduart->set_input_pin(AY31015_NP,  BIT(data, 4)); // PI - pin 35
	m_kbduart->set_input_pin(AY31015_TSB, BIT(data, 3)); // SBS - pin 36
	m_kbduart->set_input_pin(AY31015_NB2, BIT(data, 2)); // CLS2 - pin 37
	m_kbduart->set_input_pin(AY31015_NB1, BIT(data, 1)); // CLS1 - pin 38
	m_kbduart->set_input_pin(AY31015_EPS, BIT(data, 0)); // EPE - pin 39
	m_kbduart->set_input_pin(AY31015_CS, 1);
}

WRITE16_MEMBER( notetaker_state::KeyDataReset_w )
{
	m_kbduart->set_input_pin(AY31015_RDAV, 0); // DDR - pin 18
	m_kbduart->set_input_pin(AY31015_RDAV, 1); // ''
}

WRITE16_MEMBER( notetaker_state::KeyChipReset_w )
{
	m_kbduart->set_input_pin(AY31015_XR, 0); // MR - pin 21
	m_kbduart->set_input_pin(AY31015_XR, 1); // ''
}

/* EIA hd6402 */
READ16_MEMBER( notetaker_state::ReadEIAData_r )
{
	return 0xFF00||m_eiauart->get_received_data();
}

READ16_MEMBER( notetaker_state::ReadEIAStatus_r )
{
	UINT16 data = 0xFFFC;
	// note /SWE is permanently enabled, so we don't enable it here for HD6402 reading
	data |= m_eiauart->get_output_pin(AY31015_DAV ) ? 0x02 : 0; // DR - pin 19
	data |= m_eiauart->get_output_pin(AY31015_TBMT) ? 0x01 : 0; // TBRE - pin 22
	return data;
}

WRITE16_MEMBER( notetaker_state::LoadEIAData_w )
{
	m_eiauart->set_transmit_data(data&0xFF);
}

WRITE16_MEMBER( notetaker_state::LoadEIACtlReg_w )
{
	m_eiauart->set_input_pin(AY31015_CS, 0);
	m_eiauart->set_input_pin(AY31015_NP,  BIT(data, 4)); // PI - pin 35
	m_eiauart->set_input_pin(AY31015_TSB, BIT(data, 3)); // SBS - pin 36
	m_eiauart->set_input_pin(AY31015_NB2, BIT(data, 2)); // CLS2 - pin 37
	m_eiauart->set_input_pin(AY31015_NB1, BIT(data, 1)); // CLS1 - pin 38
	m_eiauart->set_input_pin(AY31015_EPS, BIT(data, 0)); // EPE - pin 39
	m_eiauart->set_input_pin(AY31015_CS, 1);
}

WRITE16_MEMBER( notetaker_state::EIADataReset_w )
{
	m_eiauart->set_input_pin(AY31015_RDAV, 0); // DDR - pin 18
	m_eiauart->set_input_pin(AY31015_RDAV, 1); // ''
}

WRITE16_MEMBER( notetaker_state::EIAChipReset_w )
{
	m_eiauart->set_input_pin(AY31015_XR, 0); // MR - pin 21
	m_eiauart->set_input_pin(AY31015_XR, 1); // ''
}


/* These next two members are memory map related for the iocpu */
READ16_MEMBER(notetaker_state::iocpu_r)
{
	UINT16 *rom = (UINT16 *)(memregion("iocpu")->base());
	rom += 0x7f800;
	UINT16 *ram = (UINT16 *)(memregion("mainram")->base());
	if ( (m_BootSeqDone == 0) || ((m_DisableROM == 0) && ((offset&0x7F800) == 0)) )
	{
		rom += (offset&0x7FF);
		return *rom;
	}
	else
	{
		ram += (offset);
		return *ram;
	}
}

WRITE16_MEMBER(notetaker_state::iocpu_w)
{
	UINT16 *ram = (UINT16 *)(memregion("mainram")->base());
	if ( (m_BootSeqDone == 0) || ((m_DisableROM == 0) && ((offset&0x7F800) == 0)) )
	{
		logerror("attempt to write %04X to ROM-mapped area at %06X ignored\n", data, offset);
		return;
	}
	ram += offset;
	*ram = data;
}

/* Address map comes from http://bitsavers.informatik.uni-stuttgart.de/pdf/xerox/notetaker/schematics/19790423_Notetaker_IO_Processor.pdf
a19 a18 a17 a16  a15 a14 a13 a12  a11 a10 a9  a8   a7  a6  a5  a4   a3  a2  a1  a0   BootSeqDone  DisableROM
mode 0: (to get the boot vector and for maybe 1 or 2 instructions afterward)
x   x   x   x    x   x   x   x    *   *   *   *    *   *   *   *    *   *   *   *    0            x          R   ROM (writes are open bus)
mode 1: (during most of boot)
0   0   0   0    0   0   0   0    *   *   *   *    *   *   *   *    *   *   *   *    1            0          R   ROM (writes are open bus)
0   0   0   0    0   0   0   1    *   *   *   *    *   *   *   *    *   *   *   *    1            0          RW  RAM
0   0   0   0    0   0   1   *    *   *   *   *    *   *   *   *    *   *   *   *    1            0          RW  RAM
<anything not all zeroes>x   x    x   x   x   x    x   x   x   x    x   x   x   x    1            0          .   Open Bus
mode 2: (during load of the emulatorcpu's firmware to the first 4k of shared ram which is on the emulatorcpu board)
0   0   0   0    0   0   *   *    *   *   *   *    *   *   *   *    *   *   *   *    1            1          RW  RAM
<anything not all zeroes>x   x    x   x   x   x    x   x   x   x    x   x   x   x    1            1          .   Open Bus

More or less:
BootSeqDone is 0, DisableROM is ignored, mem map is 0x00000-0xfffff reading is the 0x1000-long ROM, repeated every 0x1000 bytes. writing goes nowhere.
BootSeqDone is 1, DisableROM is 0,       mem map is 0x00000-0x00fff reading is the 0x1000-long ROM, remainder of memory map goes to RAM or open bus. writing the ROM area goes nowhere, writing RAM goes to RAM.
BootSeqDone is 1, DisableROM is 1,       mem map is entirely RAM or open bus for both reading and writing.
*/
static ADDRESS_MAP_START(notetaker_iocpu_mem, AS_PROGRAM, 16, notetaker_state)
	/*
	AM_RANGE(0x00000, 0x00fff) AM_ROM AM_REGION("iocpu", 0xFF000) // rom is here if either BootSeqDone OR DisableROM are zero. the 1.5 source code implies writes here are ignored
	AM_RANGE(0x01000, 0x01fff) AM_RAM // 4k of ram, local to the io processor
	AM_RANGE(0x02000, 0x3ffff) AM_RAM AM_BASE("mainram") // 256k of ram (less 8k), shared between both processors
	// note 4000-8fff? is the framebuffer for the screen?
	AM_RANGE(0xff000, 0xfffff) AM_ROM // rom is only banked in here if bootseqdone is 0, so the reset vector is in the proper place
	*/
	AM_RANGE(0x00000, 0xfffff) AM_READWRITE(iocpu_r, iocpu_w) // bypass MAME's memory map system as we need finer grained control
ADDRESS_MAP_END

/* io memory map comes from http://bitsavers.informatik.uni-stuttgart.de/pdf/xerox/notetaker/memos/19790605_Definition_of_8086_Ports.pdf
   and from the schematic at http://bitsavers.informatik.uni-stuttgart.de/pdf/xerox/notetaker/schematics/19790423_Notetaker_IO_Processor.pdf
a19 a18 a17 a16  a15 a14 a13 a12  a11 a10 a9  a8   a7  a6  a5  a4   a3  a2  a1  a0
x   x   x   x    0   x   x   x    x   x   x   0    0   0   0   x    x   x   *   .       RW  IntCon (PIC8259)
x   x   x   x    0   x   x   x    x   x   x   0    0   0   1   x    x   x   x   .       W   IPConReg
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    0   0   0   .       .   KbdInt:Open Bus
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    0   0   1   .       R   KbdInt:ReadKeyData
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    0   1   0   .       R   KbdInt:ReadOPStatus
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    0   1   1   .       .   KbdInt:Open Bus
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    1   0   0   .       W   KbdInt:LoadKeyCtlReg
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    1   0   1   .       W   KbdInt:LoadKeyData
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    1   1   0   .       W   KbdInt:KeyDataReset
x   x   x   x    0   x   x   x    x   x   x   0    0   1   0   x    1   1   1   .       W   KbdInt:KeyChipReset
x   x   x   x    0   x   x   x    x   x   x   0    0   1   1   x    x   x   x   .       W   FIFOReg
x   x   x   x    0   x   x   x    x   x   x   0    1   0   0   x    x   x   x   .       .   Open Bus(originally ReadOpStatus)
x   x   x   x    0   x   x   x    x   x   x   0    1   0   1   x    x   *   *   .       RW  SelPIA (debugger board 8255 PIA)[Open Bus on normal units]
x   x   x   x    0   x   x   x    x   x   x   0    1   1   0   x    x   x   x   .       W   FIFOBus
x   x   x   x    0   x   x   x    x   x   x   0    1   1   1   x    x   x   x   .       .   Open Bus
x   x   x   x    0   x   x   x    x   x   x   1    0   0   0   x    x   x   x   .       RW  SelDiskReg
x   x   x   x    0   x   x   x    x   x   x   1    0   0   1   x    x   *   *   .       RW  SelDiskInt
x   x   x   x    0   x   x   x    x   x   x   1    0   1   0   *    *   *   *   .       W   SelCrtInt
x   x   x   x    0   x   x   x    x   x   x   1    0   1   1   x    x   x   x   .       W   LoadDispAddr
x   x   x   x    0   x   x   x    x   x   x   1    1   0   0   x    x   x   x   .       .   Open Bus(originally ReadEIAStatus)
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    0   0   0   .       R   SelEIA:ReadEIAStatus
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    0   0   1   .       R   SelEIA:ReadEIAData
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    0   1   0   .       .   SelEIA:Open Bus
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    0   1   1   .       .   SelEIA:Open Bus
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    1   0   0   .       W   SelEIA:LoadEIACtlReg
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    1   0   1   .       W   SelEIA:LoadEIAData
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    1   1   0   .       W   SelEIA:EIADataReset
x   x   x   x    0   x   x   x    x   x   x   1    1   0   1   x    1   1   1   .       W   SelEIA:EIAChipReset
x   x   x   x    0   x   x   x    x   x   x   1    1   1   0   x    x   x   x   .       R   SelADCHi
x   x   x   x    0   x   x   x    x   x   x   1    1   1   1   x    x   x   x   .       W   CRTSwitch
*/
static ADDRESS_MAP_START(notetaker_iocpu_io, AS_IO, 16, notetaker_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00, 0x03) AM_MIRROR(0x7E1C) AM_DEVREADWRITE8("iopic8259", pic8259_device, read, write, 0x00ff)
	AM_RANGE(0x20, 0x21) AM_MIRROR(0x7E1E) AM_WRITE(IPConReg_w) // I/O processor (rom mapping, etc) control register
	AM_RANGE(0x42, 0x43) AM_MIRROR(0x7E10) AM_READ(ReadKeyData_r) // read keyboard data
	AM_RANGE(0x44, 0x45) AM_MIRROR(0x7E10) AM_READ(ReadOPStatus_r) // read keyboard fifo state
	AM_RANGE(0x48, 0x49) AM_MIRROR(0x7E10) AM_WRITE(LoadKeyCtlReg_w) // kbd uart control register
	AM_RANGE(0x4a, 0x4b) AM_MIRROR(0x7E10) AM_WRITE(LoadKeyData_w) // kbd uart data register
	AM_RANGE(0x4c, 0x4d) AM_MIRROR(0x7E10) AM_WRITE(KeyDataReset_w) // kbd uart ddr switch (data reset)
	AM_RANGE(0x4e, 0x4f) AM_MIRROR(0x7E10) AM_WRITE(KeyChipReset_w) // kbd uart reset
	//AM_RANGE(0x60, 0x61) AM_MIRROR(0x7E1E) AM_WRITE(FIFOReg_w) // DAC sample and hold and frequency setup
	//AM_RANGE(0xa0, 0xa1) AM_MIRROR(0x7E18) AM_DEVREADWRITE("debug8255", 8255_device, read, write) // debugger board 8255
	//AM_RANGE(0xc0, 0xc1) AM_MIRROR(0x7E1E) AM_WRITE(FIFOBus_w) // DAC data write to FIFO
	//AM_RANGE(0x100, 0x101) AM_MIRROR(0x7E1E) AM_WRITE(SelDiskReg_w) I/O register (adc speed, crtc pixel clock enable, +5 and +12v relays for floppy, etc)
	//AM_RANGE(0x120, 0x121) AM_MIRROR(0x7E18) AM_DEVREADWRITE("wd1791", fd1971_device) // floppy controller
	AM_RANGE(0x140, 0x15f) AM_MIRROR(0x7E00) AM_DEVREADWRITE8("crt5027", crt5027_device, read, write, 0x00FF) // crt controller
	AM_RANGE(0x1a0, 0x1a1) AM_MIRROR(0x7E10) AM_READ(ReadEIAStatus_r) // read keyboard fifo state
	AM_RANGE(0x1a2, 0x1a3) AM_MIRROR(0x7E10) AM_READ(ReadEIAData_r) // read keyboard data
	AM_RANGE(0x1a8, 0x1a9) AM_MIRROR(0x7E10) AM_WRITE(LoadEIACtlReg_w) // kbd uart control register
	AM_RANGE(0x1aa, 0x1ab) AM_MIRROR(0x7E10) AM_WRITE(LoadEIAData_w) // kbd uart data register
	AM_RANGE(0x1ac, 0x1ad) AM_MIRROR(0x7E10) AM_WRITE(EIADataReset_w) // kbd uart ddr switch (data reset)
	AM_RANGE(0x1ae, 0x1af) AM_MIRROR(0x7E10) AM_WRITE(EIAChipReset_w) // kbd uart reset
	//AM_RANGE(0x1c0, 0x1c1) AM_MIRROR(0x7E1E) AM_READ(SelADCHi_r) // ADC read
	//AM_RANGE(0x1e0, 0x1e1) AM_MIRROR(0x7E1E) AM_READ(CRTSwitch_w) // CRT power enable?
ADDRESS_MAP_END

/* iopic8259 interrupts:
irq0    parity error (parity error syndrome data will be in fffdx/fffex) - currently ignored
irq1    IPSysInt (interrupt triggered by the emulator cpu)
irq2    DiskInt (interrupt triggered by the IRQ or DRQ pins from the WD1791)
irq3    EIAInt  (interrupt triggered by the datareceived pin from the eiauart)
irq4    OddInt  (interrupt triggered by the O/E Odd/Even pin from the crt5027)
irq5    ADCInt  (interrupt triggered at the ADCSpd rate interrupt from 74c161 counter on the disk/display module to indicate adc conversion finished)
irq6    KbdInt  (interrupt triggered by the datareceived pin from the kbduart)
irq7    VSync   (interrupt from the VSYN VSync pin from the crt5027)
*/

/* writes during boot of io roms v2.0:
0x88 to port 0x020 (PCR; BootSeqDone(1), processor not locked(0), battery charger off(0), rom not disabled(0) correction off&cr4 off(1), cr3 on(0), cr2 on(0), cr1 on (0);)
0x02 to port 0x100 (IOR write: enable 5v only relay control)
0x03 to port 0x100 (IOR write: in addition to above, enable 12v relay control)
<dram memory 0x00000-0x3ffff is zeroed here>
0x13 to port 0x000 PIC (?????)
0x08 to port 0x002 PIC (UART int enabled)
0x0D to port 0x002 PIC (UART, wd1791, and parity error int enabled)
0xff to port 0x002 PIC (all ints enabled)
0x0000 to port 0x04e (reset keyboard fifo/controller)
0x0000 to port 0x1ae (reset UART)
0x0016 to port 0x048 (kbd control reg write)
0x0005 to port 0x1a8 (UART control reg write)
0x5f to port 0x140 \
0xf2 to port 0x142  \
0x7d to port 0x144   \
0x1d to port 0x146    \_ set up CRTC
0x04 to port 0x148    /
0x10 to port 0x14a   /
0x00 to port 0x154  /
0x1e to port 0x15a /
0x0a03 to port 0x100 (IOR write: set bit clock to 12Mhz)
0x2a03 to port 0x100 (IOR write: enable crtc clock chain)
0x00 to port 0x15c (fire off crtc timing chain)
read from 0x0002 (byte wide) (check interrupts) <looking for vblank int or odd/even frame int here, most likely>
0xaf to port 0x002 PIC (mask out kb int and 30hz display int)
0x0400 to 0x060 (select DAC fifo frequency 2)
read from 0x44 (byte wide) in a loop forever (read keyboard fifo status)
*/

/* Machine Reset */
void notetaker_state::machine_reset()
{
	m_BootSeqDone = 0;
	m_DisableROM = 0;
	m_kbduart->set_input_pin(AY31015_SWE, 0); // status word outputs are permanently enabled (pin 16 SFD(SWE) tied low, active)
}

/* Input ports */
static INPUT_PORTS_START( notetakr )
INPUT_PORTS_END

static MACHINE_CONFIG_START( notetakr, notetaker_state )
	/* basic machine hardware */
	/* IO CPU: 8086@8MHz */
	MCFG_CPU_ADD("iocpu", I8086, XTAL_24MHz/3) /* iD8086-2 @ E4A; 24Mhz crystal divided down to 8Mhz by i8284 clock generator */
	MCFG_CPU_PROGRAM_MAP(notetaker_iocpu_mem)
	MCFG_CPU_IO_MAP(notetaker_iocpu_io)
	MCFG_CPU_IRQ_ACKNOWLEDGE_DEVICE("iopic8259", pic8259_device, inta_cb)
	MCFG_PIC8259_ADD("iopic8259", INPUTLINE("iocpu", 0), VCC, NULL) // iP8259A-2 @ E6

	/* Emulator CPU: 8086@5MHz */
	/*MCFG_CPU_ADD("emulatorcpu", I8086, XTAL_15MHz/3)
	MCFG_CPU_PROGRAM_MAP(notetaker_emulatorcpu_mem)
	MCFG_CPU_IO_MAP(notetaker_emulatorcpu_io)
	MCFG_CPU_IRQ_ACKNOWLEDGE_DEVICE("emulatorpic8259", pic8259_device, inta_cb)
	MCFG_PIC8259_ADD("emulatorpic8259", INPUTLINE("emulatorcpu", 0), VCC, NULL) // iP8259A-2 @ E6
	*/

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(250))
	MCFG_SCREEN_UPDATE_DRIVER(notetaker_state, screen_update)
	MCFG_SCREEN_SIZE(64*6, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0, 64*6-1, 0, 32*8-1)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_PALETTE_ADD_BLACK_AND_WHITE("palette")

	MCFG_DEVICE_ADD( "crt5027", CRT5027, XTAL_36MHz/14) // the clock for the crt5027 is configurable rate; 36MHz xtal divided by 1*,2,4,6,8,10,12, or 14 (* because this setting may actually oscillate unstably, or not clock at all)
	MCFG_VIDEO_SET_SCREEN("screen")

	MCFG_DEVICE_ADD( "kbduart", AY31015, 0 )
	MCFG_AY31015_RX_CLOCK(XTAL_960kHz) // hard-wired to 960KHz xtal #f11 (60000 baud, 16 clocks per baud)
	MCFG_AY31015_TX_CLOCK(XTAL_960kHz) // hard-wired to 960KHz xtal #f11 (60000 baud, 16 clocks per baud)

	MCFG_DEVICE_ADD( "eiauart", AY31015, 0 )
	MCFG_AY31015_RX_CLOCK((96000/4)/5) // hard-wired through an mc14568b divider set to divide by 4, the result set to divide by 5; this resulting 4800hz signal being 300 baud (16 clocks per baud)
	MCFG_AY31015_TX_CLOCK((96000/4)/5) // hard-wired through an mc14568b divider set to divide by 4, the result set to divide by 5; this resulting 4800hz signal being 300 baud (16 clocks per baud)
	/* Devices */

MACHINE_CONFIG_END

DRIVER_INIT_MEMBER(notetaker_state,notetakr)
{
	// descramble the rom; the whole thing is a gigantic scrambled mess either to ease
	// interfacing with older xerox technologies which used A0 and D0 as the MSB bits
	// or maybe because someone screwed up somewhere along the line. we may never know.
	// see http://bitsavers.informatik.uni-stuttgart.de/pdf/xerox/notetaker/schematics/19790423_Notetaker_IO_Processor.pdf pages 12 and onward
	UINT16 *romsrc = (UINT16 *)(memregion("iocpuload")->base());
	UINT16 *romdst = (UINT16 *)(memregion("iocpu")->base());
	UINT16 *temppointer;
	UINT16 wordtemp;
	UINT16 addrtemp;
		// leave the src pointer alone, since we've only used a 0x1000 long address space
		romdst += 0x7f800; // set the dest pointer to 0xff000 (>>1 because 16 bits data)
		for (int i = 0; i < 0x800; i++)
		{
			wordtemp = BITSWAP16(*romsrc, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15); // data bus is completely reversed
			addrtemp = BITSWAP16(i, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10); // address bus is completely reversed; 11-15 should always be zero
			temppointer = romdst+(addrtemp&0x7FF);
			*temppointer = wordtemp;
			romsrc++;
		}
}

/* ROM definition */
ROM_START( notetakr )
	ROM_REGION( 0x1000, "iocpuload", ROMREGION_ERASEFF ) // load roms here before descrambling
	ROM_SYSTEM_BIOS( 0, "v2.00", "IO Monitor v2.00" ) // dumped from Notetaker
	ROMX_LOAD( "biop__2.00_hi.b2716.h1", 0x0000, 0x0800, CRC(1119691d) SHA1(4c20b595b554e6f5489ab2c3fb364b4a052f05e3), ROM_SKIP(1) | ROM_BIOS(1))
	ROMX_LOAD( "biop__2.00_lo.b2716.g1", 0x0001, 0x0800, CRC(b72aa4c7) SHA1(85dab2399f906c7695dc92e7c18f32e2303c5892), ROM_SKIP(1) | ROM_BIOS(1))
	ROM_SYSTEM_BIOS( 1, "v1.50", "IO Monitor v1.50" ) // typed from the source listing at http://bitsavers.trailing-edge.com/pdf/xerox/notetaker/memos/19790620_Z-IOP_1.5_ls.pdf and scrambled
	ROMX_LOAD( "z-iop_1.50_hi.h1", 0x0000, 0x0800, CRC(2994656e) SHA1(ca2bb38eb9075c5c2f3cc5439b209e7e216084da), ROM_SKIP(1) | ROM_BIOS(2))
	ROMX_LOAD( "z-iop_1.50_lo.g1", 0x0001, 0x0800, CRC(2cb79a67) SHA1(692aafd2aeea27533f6288dbb1cb8678ea08fade), ROM_SKIP(1) | ROM_BIOS(2))
	ROM_REGION( 0x100000, "iocpu", ROMREGION_ERASEFF ) // area for descrambled roms
	ROM_REGION( 0x100000, "mainram", ROMREGION_ERASEFF ) // ram cards
	ROM_REGION( 0x1000, "proms", ROMREGION_ERASEFF )
	ROM_LOAD( "disksep.prom.82s147.a4", 0x000, 0x200, NO_DUMP ) // disk data separator prom from the disk/display module board
	ROM_LOAD( "memcasraswrite.prom.82s147.b1", 0x200, 0x200, NO_DUMP ) // memory cas/ras/write prom from the memory address logic board
	ROM_LOAD( "setmemrq.prom.82s126.d9", 0x400, 0x100, NO_DUMP ) // SETMEMRQ memory timing prom from the disk/display module board; The equations for this one are actually listed on the schematic and the prom dump can be generated from these:
	/*
	SetMemRq:
	Address:
	01234567
	|||||||\- RCtr.3 (LSB)
	||||||\-- RCtr.2
	|||||\--- RCtr.1
	||||\---- RCtr.0 (MSB)
	|||\----- WCtr.3 (LSB)
	||\------ WCtr.2
	|\------- WCtr.1
	\-------- WCtr.0 (MSB)

	Data:
	0123
	|\\\- N/C (zero?)
	\---- SetMemRq

	Equation: SETMEMRQ == (
	  ((Rctr == 0) && ((Wctr == 0)||(Wctr == 4)||(Wctr == 8)))
	||((Rctr == 4) && ((Wctr == 4)||(Wctr == 8)||(Wctr == 12)))
	||((Rctr == 8) && ((Wctr == 8)||(Wctr == 12)||(Wctr == 0)))
	||((Rctr == 12) && ((Wctr == 12)||(Wctr == 0)||(Wctr == 4)))
	)
	(the setmemrq output might be inverted, as well)
	*/
ROM_END

/* Driver */

/*    YEAR      NAME  PARENT  COMPAT   MACHINE     INPUT            STATE      INIT  COMPANY     FULLNAME                FLAGS */
COMP( 1978, notetakr,      0,      0, notetakr, notetakr, notetaker_state, notetakr, "Xerox", "Notetaker", MACHINE_IS_SKELETON)
//COMP( 1978, notetakr,      0,      0, notetakr, notetakr, driver_device, notetakr, "Xerox", "Notetaker", MACHINE_IS_SKELETON)
