/******************************************************************************
*
*  Telesensory Systems Inc./Speech Plus
*  1500 and 2000 series
*  Prose 2020
*  By Jonathan Gevaryahu AKA Lord Nightmare and Kevin 'kevtris' Horton
*
*  Skeleton Driver
*
*  DONE:
*  Skeleton Written
*  Load cpu and dsp roms and mapper proms
*  Successful compile
*  Successful run
*  Correctly Interleave 8086 CPU roms
*  Debug LEDs hooked to popmessage
*  Correctly load UPD7720 roms as UPD7725 data - done, this is utterly disgusting code.
*
*  TODO:
*  Correctly implement UPD7720 cpu core to avoid needing revolting conversion code
*  Correct memory maps and io maps - partly done
*  Attach peripherals, timers and uarts
*  Add dipswitches and jumpers
*  Hook terminal to serial UARTS
*  Everything else 
*
*  Notes:
*  Text in rom indicates there is a test mode 'activated by switch s4 dash 7'
******************************************************************************/
#define ADDRESS_MAP_MODERN

/* Core includes */
#include "emu.h"
#include "includes/tsispch.h"
#include "cpu/upd7725/upd7725.h"
#include "cpu/i86/i86.h"
#include "machine/terminal.h"

static GENERIC_TERMINAL_INTERFACE( tsispch_terminal_intf )
{
	DEVCB_NULL // for now...
};

WRITE16_MEMBER( tsispch_state::led_w )
{
	tsispch_state *state = machine->driver_data<tsispch_state>();
	UINT16 data2 = data >> 8;
	state->statusLED = data2&0xFF;
	//fprintf(stderr,"0x03400 LED write: %02X\n", data);
	//popmessage("LED status: %02X\n", data2&0xFF);
#ifdef VERBOSE
	logerror("8086: LED status: %02X\n", data2&0xFF);
#endif
	popmessage("LED status: %x %x %x %x %x %x %x %x\n", BIT(data2,7), BIT(data2,6), BIT(data2,5), BIT(data2,4), BIT(data2,3), BIT(data2,2), BIT(data2,1), BIT(data2,0));
}

/* Reset */
void tsispch_state::machine_reset()
{
	// clear fifos (TODO: memset would work better here...)
	int i;
	for (i=0; i<32; i++) infifo[i] = 0;
	infifo_tail_ptr = infifo_head_ptr = 0;
	fprintf(stderr,"machine reset\n");
}

DRIVER_INIT( prose2k )
{
	UINT8 *dspsrc = (UINT8 *)machine->region("dspprgload")->base();
	UINT32 *dspprg = (UINT32 *)machine->region("dspprg")->base();
	fprintf(stderr,"driver init\n");
    // unpack 24 bit data into 32 bit space
	// TODO: unpack such that it can actually RUN as upd7725 code; this requires
	//       some shuffling:
	// data format as-is in dspsrc:
	// bit 7  6  5  4  3  2  1  0
	// b1  15 16 17 18 19 20 21 22 >- needs to be flipped around
	// b2  XX 8  9  10 11 12 13 14 >- needs to be flipped around and the zero moved to bit 4
	UINT8 byte1;
	UINT8 byte2;
	UINT8 byte3;
        for (int i = 0; i < 0x600; i+= 3)
        {
			byte1 = BIT(dspsrc[0+i],0)<<7;
			byte1 |= BIT(dspsrc[0+i],1)<<6;
			byte1 |= BIT(dspsrc[0+i],2)<<5;
			byte1 |= BIT(dspsrc[0+i],3)<<4;
			byte1 |= BIT(dspsrc[0+i],4)<<3;
			byte1 |= BIT(dspsrc[0+i],5)<<2;
			byte1 |= BIT(dspsrc[0+i],6)<<1;
			byte1 |= BIT(dspsrc[0+i],7)<<0;
			// here's where things get disgusting: if the first byte was an OP or RT, do the following:
			if ((byte1&0x80) == 0x00)
			{
				byte2 = BIT(dspsrc[1+i],0)<<7;
				byte2 |= BIT(dspsrc[1+i],1)<<6;
				byte2 |= BIT(dspsrc[1+i],2)<<5;
				byte2 |= BIT(dspsrc[1+i],7)<<4; // should be always 0
				byte2 |= BIT(dspsrc[1+i],3)<<3;
				byte2 |= BIT(dspsrc[1+i],4)<<2;
				byte2 |= BIT(dspsrc[1+i],5)<<1;
				byte2 |= BIT(dspsrc[1+i],6)<<0;
				
				byte3 = BIT(dspsrc[2+i],0)<<7;
				byte3 |= BIT(dspsrc[2+i],1)<<6;
				byte3 |= BIT(dspsrc[2+i],2)<<5;
				byte3 |= BIT(dspsrc[2+i],3)<<4;
				byte3 |= BIT(dspsrc[2+i],4)<<3;
				byte3 |= BIT(dspsrc[2+i],5)<<2;
				byte3 |= BIT(dspsrc[2+i],6)<<1;
				byte3 |= BIT(dspsrc[2+i],7)<<0;
			}
			else if ((byte1&0xC0) == 0x80) // jp instruction
			{
				byte2 = BIT(dspsrc[1+i],0)<<7; // bit 15 goes to bit 15
				byte2 |= BIT(dspsrc[1+i],1)<<6; // bit 14 goes to bit 14
				byte2 |= BIT(dspsrc[1+i],7)<<5; // 0 goes to bit 13
				byte2 |= BIT(dspsrc[1+i],7)<<4; // 0 goes to bit 12
				byte2 |= BIT(dspsrc[1+i],7)<<3; // 0 goes to bit 11
				byte2 |= BIT(dspsrc[1+i],2)<<2; // bit 12 goes to bit 10
				byte2 |= BIT(dspsrc[1+i],3)<<1; // bit 11 goes to bit 9
				byte2 |= BIT(dspsrc[1+i],4)<<0; // bit 10 goes to bit 8
				
				byte3 = BIT(dspsrc[1+i],5)<<7; // bit 9 goes to bit 7
				byte3 |= BIT(dspsrc[1+i],6)<<6; // bit 8 goes to bit 6
				byte3 |= BIT(dspsrc[2+i],0)<<5; // bit 7 goes to bit 5
				byte3 |= BIT(dspsrc[2+i],1)<<4; // bit 6 goes to bit 4
				byte3 |= BIT(dspsrc[2+i],2)<<3; // bit 5 goes to bit 3
				byte3 |= BIT(dspsrc[2+i],3)<<2; // bit 4 goes to bit 2
				byte3 |= BIT(dspsrc[2+i],6)<<1;
				byte3 |= BIT(dspsrc[2+i],7)<<0;
			}
			else // ld instruction
			{
				byte2 = BIT(dspsrc[1+i],0)<<7; // bit 15 goes to bit 15
				byte2 |= BIT(dspsrc[1+i],1)<<6; // bit 14 goes to bit 14
				byte2 |= BIT(dspsrc[1+i],2)<<5; // bit 13 goes to bit 13
				byte2 |= BIT(dspsrc[1+i],3)<<4; // bit 12 goes to bit 12
				byte2 |= BIT(dspsrc[1+i],4)<<3; // bit 11 goes to bit 11
				byte2 |= BIT(dspsrc[1+i],5)<<2; // bit 10 goes to bit 10
				byte2 |= BIT(dspsrc[1+i],6)<<1; // bit 9 goes to bit 9
				byte2 |= BIT(dspsrc[2+i],0)<<0; // bit 8 goes to bit 8
				
				byte3 = BIT(dspsrc[2+i],1)<<7;  // bit 7 goes to bit 7
				byte3 |= BIT(dspsrc[2+i],2)<<6;  // bit 6 goes to bit 6
				byte3 |= BIT(dspsrc[2+i],3)<<5;  // 0 goes to bit 5
				byte3 |= BIT(dspsrc[2+i],3)<<4;  // 0 goes to bit 4
				byte3 |= BIT(dspsrc[2+i],4)<<3;  // bit 3 goes to bit 3
				byte3 |= BIT(dspsrc[2+i],5)<<2;  // bit 2 goes to bit 2
				byte3 |= BIT(dspsrc[2+i],6)<<1;  // bit 1 goes to bit 1
				byte3 |= BIT(dspsrc[2+i],7)<<0;  // bit 0 goes to bit 0
			}

            *dspprg = byte1<<24 | byte2<<16 | byte3<<8;
            dspprg++;
        }
}

/******************************************************************************
 Address Maps
******************************************************************************/
/* The address map of the prose 2020 is controlled by 2 proms, see the rom section
   for details on those.
   (x = ignored; * = selects address within this range; s = selects one of a pair of chips)
   A19 A18 A17 A16  A15 A14 A13 A12  A11 A10 A9 A8  A7 A6 A5 A4  A3 A2 A1 A0
     0   0   x   x    0   x   0   *    *   *  *  *   *  *  *  *   *  *  *  s  6264*2 SRAM first half
     0   0   x   x    0   x   1   0    *   *  *  *   *  *  *  *   *  *  *  s  6264*2 SRAM 3rd quarter
     0   0   x   x    0   x   1   1    0   0  0  x   x  x  x  x   x  x  *  x  iP8251A @ U15
     0   0   x   x    0   x   1   1    0   0  1  x   x  x  x  x   x  x  *  x  AMD P8259A PIC
     0   0   x   x    0   x   1   1    0   1  0  ?   ?  ?  ?  ?   ?  ?  ?  ?  LEDS and dipswitches?
     0   0   x   x    0   x   1   1    0   1  1  x   x  x  x  x   x  x  *  x  UPD77P20 data/status
     0   0   x   x    0   x   1   1    1                                      Open bus?
     0   0   x   x    1   x                                                   Open bus?
     0   1                                                                    Open bus?
     1   0                                                                    Open bus?
     1   1   0   *    *   *   *   *    *   *  *  *   *  *  *  *   *  *  *  s  ROMs 2 and 3
     1   1   1   *    *   *   *   *    *   *  *  *   *  *  *  *   *  *  *  s  ROMs 0 and 1
*/
static ADDRESS_MAP_START(i8086_mem, ADDRESS_SPACE_PROGRAM, 16, tsispch_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00000, 0x02FFF) AM_MIRROR(0x34000) AM_RAM // verified; 6264*2 sram, only first 3/4 used
	//AM_RANGE(0x03000, 0x03003) AM_MIRROR(0x341FD) // iP8251A@U15
	//AM_RANGE(0x03202, 0x03203) AM_MIRROR(0x341FD) // AMD P8259 PIC @ U5
	AM_RANGE(0x03400, 0x03401) AM_MIRROR(0x34000) AM_WRITE(led_w) // write is 8 bits, but there are only 4 debug leds; other 4 bits may select a bank of dipswitches to be read here?
	//AM_RANGE(0x03600, 0x03601) AM_MIRROR(0x341FD) // UPD77P20 data reg r/w
	//AM_RANGE(0x03602, 0x03603) AM_MIRROR(0x341FD) // UPD77P20 status reg r
	AM_RANGE(0xc0000, 0xfffff) AM_ROM // verified
ADDRESS_MAP_END

static ADDRESS_MAP_START(i8086_io, ADDRESS_SPACE_IO, 16, tsispch_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x990, 0x991) AM_NOP // wrong; to force correct compile
ADDRESS_MAP_END

static ADDRESS_MAP_START(dsp_prg_map, ADDRESS_SPACE_PROGRAM, 32, tsispch_state)
    AM_RANGE(0x0000, 0x01ff) AM_ROM AM_REGION("dspprg", 0)
ADDRESS_MAP_END

static ADDRESS_MAP_START(dsp_data_map, ADDRESS_SPACE_DATA, 16, tsispch_state)
    AM_RANGE(0x0000, 0x01ff) AM_ROM AM_REGION("dspdata", 0)
ADDRESS_MAP_END


/******************************************************************************
 Input Ports
******************************************************************************/
static INPUT_PORTS_START( prose2k )
INPUT_PORTS_END

/******************************************************************************
 Machine Drivers
******************************************************************************/
static MACHINE_CONFIG_START( prose2k, tsispch_state )
    /* basic machine hardware */
    MCFG_CPU_ADD("maincpu", I8086, 4772720) /* TODO: correct clock speed */
    MCFG_CPU_PROGRAM_MAP(i8086_mem)
    MCFG_CPU_IO_MAP(i8086_io)

    MCFG_CPU_ADD("dsp", UPD7725, 8000000) /* TODO: correct clock and correct dsp type is UPD77P20 */
    MCFG_CPU_PROGRAM_MAP(dsp_prg_map)
    MCFG_CPU_DATA_MAP(dsp_data_map)

    /* sound hardware */
    //MCFG_SPEAKER_STANDARD_MONO("mono")
    //MCFG_SOUND_ADD("dac", DAC, 0) /* TODO: correctly figure out how the DAC works */
    //MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)

    MCFG_FRAGMENT_ADD( generic_terminal )
    MCFG_GENERIC_TERMINAL_ADD(TERMINAL_TAG,tsispch_terminal_intf)
MACHINE_CONFIG_END

/******************************************************************************
 ROM Definitions
******************************************************************************/
ROM_START( prose2k )
	ROM_REGION(0x100000,"maincpu", 0)
	// prose 2000/2020 firmware version 3.4.1
	ROMX_LOAD( "v3.4.1__2000__2.u22",   0xc0000, 0x10000, CRC(201D3114) SHA1(549EF1AA28D5664D4198CBC1826B31020D6C4870),ROM_SKIP(1))
	ROMX_LOAD( "v3.4.1__2000__3.u45",   0xc0001, 0x10000, CRC(190C77B6) SHA1(2B90B3C227012F2085719E6283DA08AFB36F394F),ROM_SKIP(1))
	ROMX_LOAD( "v3.4.1__2000__0.u21",   0xe0000, 0x10000, CRC(3FAE874A) SHA1(E1D3E7BA309B29A9C3EDBE3D22BECF82EAE50A31),ROM_SKIP(1))
	ROMX_LOAD( "v3.4.1__2000__1.u44",   0xe0001, 0x10000, CRC(BDBB0785) SHA1(6512A8C2641E032EF6BB0889490D82F5D4399575),ROM_SKIP(1))

	// TSI/Speech plus DSP firmware v3.12 8/9/88, NEC UPD77P20
	ROM_REGION( 0x600, "dspprgload", 0) // packed 24 bit data
	ROM_LOAD( "v3.12__8-9-88__dsp_prog.u29", 0x0000, 0x0600, CRC(9E46425A) SHA1(80A915D731F5B6863AEEB448261149FF15E5B786))
	ROM_REGION( 0x800, "dspprg", ROMREGION_ERASEFF) // for unpacking 24 bit data into
	ROM_REGION( 0x400, "dspdata", 0)
	ROM_LOAD( "v3.12__8-9-88__dsp_data.u29", 0x0000, 0x0400, CRC(F4E4DD16) SHA1(6E184747DB2F26E45D0E02907105FF192E51BABA))

	// mapping proms:
	// All are am27s19 32x8 TriState PROMs (equivalent to 82s123/6331)
	// L - always low; H - always high
	// U77: unknown (what does this do?); input is ?
	//      output bits 0bLLLLzyxH
	//      x - unknown
	//      y - unknown
	//      z - unknown
	//
	// U79: SRAM and peripheral mapping:
	//      input is A19 for I4, A18 for I3, A15 for I2, A13 for I1, A12 for I0 (NEEDS VERIFY)
	//      On the Prose 2000 board dumped, only bits 3 and 0 are used;
	//      bits 7-4 are always low, bits 2 and 1 are always high.
	//      SRAMS are only populated in U61 and U64.
	//      output bits 0bLLLLyHHx
	//      3 - to /EN3 (pin 4) of 74S138N at U80
	//          AND to EN1 (pin 6) of 74S138N at U78
	//          i.e. one is activated when pin is high and other when pin is low
	//          The 74S138N at U80:
	//              /EN2 - pulled to GND
	//              EN1 - pulled to VCC through resistor R5
	//              inputs: S0 - A9; S1 - A10; S2 - A11
	//              /Y0 - /CS (pin 11) of iP8251A at U15
	//              /Y1 - /CS (pin 1) of AMD 8259A at U4
	//              /Y2 - pins 1, 4, 9 (1A, 2A, 3A inputs) of 74HCT32 Quad OR gate at U58 <wip, involves LEDS?>
	//              /Y3 - pin 26 (/CS) of UPD77P20 at U29
	//              /Y4 thru /Y7 - seem unconnected SO FAR? <wip>
	//          The 74S138N at U78: <wip>
	//              /EN3 - ?
	//              /EN2 - ?
	//              inputs: ?
	//              /Y3 - connects somewhere
	//              <wip>
	//      2 - to /CS1 on 6264 SRAMs at U63 and U66
	//      1 - to /CS1 on 6264 SRAMs at U62 and U65
	//      0 - to /CS1 on 6264 SRAMs at U61 and U64
	//
	// U81: maps ROMS: input is A19-A15 for I4,3,2,1,0
	//      On the Prose 2000 board dumped, only bits 6 and 5 are used,
	//      the rest are always high; maps roms 0,1,2,3 to C0000-FFFFF.
	//      The Prose 2000 board has empty unpopulated sockets for roms 4-15;
	//      if present these would be driven by a different prom in this location.
	//      bit - function
	//      7 - to /CE of ROMs 14(U28) and 15(U51)
	//      6 - to /CE of ROMs 0(U21) and 1(U44)
	//      5 - to /CE of ROMs 2(U22) and 3(U45)
	//      4 - to /CE of ROMs 4(U23) and 5(U46)
	//      3 - to /CE of ROMs 6(U24) and 7(U47)
	//      2 - to /CE of ROMs 8(U25) and 9(U48)
	//      1 - to /CE of ROMs 10(U26) and 11(U49)
	//      0 - to /CE of ROMs 12(U27) and 13(U50)
	ROM_REGION(0x1000, "proms", 0)
	ROM_LOAD( "am27s19.u77", 0x0000, 0x0020, CRC(A88757FC) SHA1(9066D6DBC009D7A126D75B8461CA464DDF134412))
	ROM_LOAD( "am27s19.u79", 0x0020, 0x0020, CRC(A165B090) SHA1(BFC413C79915C68906033741318C070AD5DD0F6B))
	ROM_LOAD( "am27s19.u81", 0x0040, 0x0020, CRC(62E1019B) SHA1(ACADE372EDB08FD0DCB1FA3AF806C22C47081880))
	ROM_END


/******************************************************************************
 Drivers
******************************************************************************/

/*    YEAR  NAME	PARENT	COMPAT	MACHINE		INPUT	INIT	COMPANY     FULLNAME            FLAGS */
COMP( 1985, prose2k,	0,		0,		prose2k,		prose2k,	prose2k,	"Telesensory Systems Inc/Speech Plus",	"Prose 2000/2020",	GAME_NOT_WORKING | GAME_NO_SOUND )
