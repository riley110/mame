/*

Sega System SP (Spider)
skeleton driver

this is another 'Naomi-derived' system

as for SP... it must be done having few things in mind:
differences with Naomi in SH4 memory map only:
0x00200000 - 0x00207fff - no SRAM here
0x01000000 - 0x0100ffff - banked access to ROM board address space, see later
0x01010000 - 0x010101ff - I/O registers, IRQ control, reg 0 - upper 16bits of rombd bank address

as for ROM board - unlike regular Naomi M4 it have not only ROM itself,
but the other hardware too*:
0x00000000 - 0x1fffffff - FlashROM, like in regular Naomi M4 cart
0x39xxxxxx - SRAM(32Kb)**
0x3axxxxxx - CF IDE registers (0 - data, 4 - error, 8 - sector count, etc)
0x3bxxxxxx - CF IDE AltStatus/Device Ctrl register
0x3dxxxxxx - Network aka Media board shared buffer/RAM
0x3fxxxxxx - Network board present flag (0x01)**

*note: M4-decryption works for all areas if address bit 30 is 1,
for example BIOS can read CF IDE data with on-fly data decryption

**note: this is 8bit device on 16bit bus, each even byte access actual
data, each odd byte is 0xFF.

so, unlike regular Naomi this "rom board" can be accessed via BOTH regular
G1 bus PIO or DMA, or directly via banked  area 0x0100xxxx in SH4 address space.



todo: make this actually readable, we don't support unicode source files

 Title                                       PCB ID	 REV	CF ID		Dumped	Region	PIC             MAIN BD Serial
Battle Police                               ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Beetle DASH!!                               ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Bingo Galaxy                                ???-?????				no		???-????-????   AAFE-01E10924916, AAFE-01D67304905, Medal
Bingo Parade                                ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx, Medal
Brick People / Block People                 834-14881				ROM	ALL	253-5508-0558   AAFE-01F67905202, AAFE-01F68275202
Dinosaur King                               834-14493-01 D			ROM	US	253-5508-0408   AAFE-01D1132xxxx, AAFE-01D15924816
Dinosaur King - Operation: Dinosaur Rescue  837-14434-91	MDA-C0021?	ROM	US/EXP	253-5508-0408   AAFE-01A30164715, AAFE-01B92094811
-                                           834-14662-01
Dinosaur King 2                             ???-?????				no		253-5508-0408   AAFE-xxxxxxxxxxx
Dinosaur King 2 Ver 2.5                     834-14792-02 F	MDA-C0047	CF	EXP	253-5508-0408   AAFE-01D73384904
Disney: Magical Dream Dance on Stage        ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Future Police Patrol Chase                  ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Issyouni Turbo Drive                        ???-?????				no		???-????-????   AAFE-01E91305101
Issyouni Wan Wan                            ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
King of Beetle: Battle Terminal             ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Love & Berry Ver 1.003                      834-14661-02			ROM	EXP	253-5508-0446   AAFE-01D84934906
Love & Berry Ver 2.000                      834-14661-02			ROM	EXP	253-5508-0446   AAFE-01D8493xxxx
Love & Berry 3 EXP Ver 1.002                834-14661-01	MDA-C0042	CF	US/EXP	253-5508-0446   AAFE-01D64704904
Marine & Marine                             ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Mirage World                                ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx, Medal
Monopoly: The Medal                         ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx, Medal
Monopoly: The Medal 2nd Edition             ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx, Medal
Mushiking 2K6 2ND                           ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Mushiking 2K7 1ST                           ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx
Tetris Giant / Tetris Dekaris               834-14970	 G	MDA-C0076	CF	ALL	253-5508-0604   AAFE-01G03025212
Tetris Giant / Tetris Dekaris Ver.2.000     834-14970	 G			ROM	ALL	253-5508-0604   AAFE-xxxxxxxxxxx
Thomas: The Tank Engine                     ???-?????				no		???-????-????   AAFE-xxxxxxxxxxx

REV PCB       IC6s      Flash       AU1500
D  171-8278D  315-6370  8x 128Mbit  AMD
F  171-8278F  315-6416  8x 512Mbit  AMD
G  171-8278G  315-6416  2x 512Mbit  RMI

*/

#include "emu.h"
#include "cpu/sh4/sh4.h"
#include "debugger.h"
#include "includes/segasp.h"


// todo, base DC / Naomi stuff should be in it's own map, differences only here, same for Naomi 2 etc.
static ADDRESS_MAP_START( segasp_map, AS_PROGRAM, 64, segasp_state )
	/* Area 0 */
	AM_RANGE(0x00000000, 0x001fffff) AM_MIRROR(0xa2000000) AM_ROM AM_REGION("maincpu", 0) // BIOS

	AM_RANGE(0x005f6800, 0x005f69ff) AM_MIRROR(0x02000000) AM_READWRITE(dc_sysctrl_r, dc_sysctrl_w )
	AM_RANGE(0x005f6c00, 0x005f6cff) AM_MIRROR(0x02000000) AM_DEVICE32( "maple_dc", maple_dc_device, amap, U64(0xffffffffffffffff) )
	AM_RANGE(0x005f7000, 0x005f70ff) AM_MIRROR(0x02000000) AM_DEVICE16( "rom_board", naomi_board, submap, U64(0x0000ffff0000ffff) )
	AM_RANGE(0x005f7400, 0x005f74ff) AM_MIRROR(0x02000000) AM_DEVICE32( "rom_board", naomi_g1_device, amap, U64(0xffffffffffffffff) )
	AM_RANGE(0x005f7800, 0x005f78ff) AM_MIRROR(0x02000000) AM_READWRITE(dc_g2_ctrl_r, dc_g2_ctrl_w )
	AM_RANGE(0x005f7c00, 0x005f7cff) AM_MIRROR(0x02000000) AM_DEVICE32("powervr2", powervr2_device, pd_dma_map, U64(0xffffffffffffffff))
	AM_RANGE(0x005f8000, 0x005f9fff) AM_MIRROR(0x02000000) AM_DEVICE32("powervr2", powervr2_device, ta_map, U64(0xffffffffffffffff))
	AM_RANGE(0x00600000, 0x006007ff) AM_MIRROR(0x02000000) AM_READWRITE(dc_modem_r, dc_modem_w )
	AM_RANGE(0x00700000, 0x00707fff) AM_MIRROR(0x02000000) AM_READWRITE32(dc_aica_reg_r, dc_aica_reg_w, U64(0xffffffffffffffff))
	AM_RANGE(0x00710000, 0x0071000f) AM_MIRROR(0x02000000) AM_DEVREADWRITE16("aicartc", aicartc_device, read, write, U64(0x0000ffff0000ffff) )
	
	AM_RANGE(0x00800000, 0x00ffffff) AM_MIRROR(0x02000000) AM_READWRITE(naomi_arm_r, naomi_arm_w )           // sound RAM (8 MB)
	   
	/* External Device */
	AM_RANGE(0x01000000, 0x0100ffff) AM_RAM // - banked access to ROM board address space
//	AM_RANGE(0x01010000, 0x010101ff) // I/O regs

	/* Area 1 */
	AM_RANGE(0x04000000, 0x04ffffff) AM_MIRROR(0x02000000) AM_RAM AM_SHARE("dc_texture_ram")      // texture memory 64 bit access
	AM_RANGE(0x05000000, 0x05ffffff) AM_MIRROR(0x02000000) AM_RAM AM_SHARE("frameram") // apparently this actually accesses the same memory as the 64-bit texture memory access, but in a different format, keep it apart for now

	/* Area 2*/
	AM_RANGE(0x08000000, 0x09ffffff) AM_MIRROR(0x02000000) AM_NOP // 'Unassigned'

	/* Area 3 */
	AM_RANGE(0x0c000000, 0x0dffffff) AM_MIRROR(0xa2000000) AM_RAM AM_SHARE("dc_ram")

	/* Area 4 */
	AM_RANGE(0x10000000, 0x107fffff) AM_MIRROR(0x02000000) AM_DEVWRITE("powervr2", powervr2_device, ta_fifo_poly_w)
	AM_RANGE(0x10800000, 0x10ffffff) AM_DEVWRITE8("powervr2", powervr2_device, ta_fifo_yuv_w, U64(0xffffffffffffffff))
	AM_RANGE(0x11000000, 0x11ffffff) AM_DEVWRITE("powervr2", powervr2_device, ta_texture_directpath0_w) // access to texture / framebuffer memory (either 32-bit or 64-bit area depending on SB_LMMODE0 register - cannot be written directly, only through dma / store queue)
	/*       0x12000000 -0x13ffffff Mirror area of  0x10000000 -0x11ffffff */
	AM_RANGE(0x13000000, 0x13ffffff) AM_DEVWRITE("powervr2", powervr2_device, ta_texture_directpath1_w) // access to texture / framebuffer memory (either 32-bit or 64-bit area depending on SB_LMMODE1 register - cannot be written directly, only through dma / store queue)

	/* Area 5 */
	//AM_RANGE(0x14000000, 0x17ffffff) AM_NOP // MPX Ext.

	/* Area 6 */
	//AM_RANGE(0x18000000, 0x1bffffff) AM_NOP // Unassigned

	/* Area 7 */
	//AM_RANGE(0x1c000000, 0x1fffffff) AM_NOP // SH4 Internal
ADDRESS_MAP_END

INPUT_PORTS_START( segasp )
	PORT_INCLUDE( naomi )
INPUT_PORTS_END

static MACHINE_CONFIG_DERIVED_CLASS( segasp, naomi_base, segasp_state )
	MCFG_CPU_MODIFY("maincpu")
	MCFG_CPU_PROGRAM_MAP(segasp_map)

// todo, not exactly NaomiM4 (see notes at top of driver) use custom board type here instead
	MCFG_NAOMI_M4_BOARD_ADD("rom_board", ":pic_readout", "naomibd_eeprom", ":boardid", WRITE8(dc_state, g1_irq))
MACHINE_CONFIG_END

#define ROM_LOAD16_WORD_SWAP_BIOS(bios,name,offset,length,hash) \
		ROMX_LOAD(name, offset, length, hash, ROM_GROUPWORD | ROM_BIOS(bios+1)) /* Note '+1' */

#define SEGASP_BIOS \
	ROM_REGION( 0x200000, "maincpu", 0) \
	ROM_SYSTEM_BIOS( 0, "bios0", "BOOT VER 1.01" ) \
	ROM_LOAD16_WORD_SWAP_BIOS( 0, "epr-24236a.ic50", 0x000000, 0x200000, CRC(ca7df0de) SHA1(504c74d5fc96c53ef9f7753e9e37fb8b39cb628c) ) \
	ROM_SYSTEM_BIOS( 1, "bios1", "BOOT VER 2.00" ) \
	ROM_LOAD16_WORD_SWAP_BIOS( 1, "epr-24328.ic50", 0x000000, 0x200000, CRC(25f2ef00) SHA1(e58dec9f171e52b3ded213b3fcd9a0de8a438076) ) \
	ROM_SYSTEM_BIOS( 2, "bios2", "BOOT VER 2.01" ) \
	ROM_LOAD16_WORD_SWAP_BIOS( 2, "epr-24328a.ic50", 0x000000, 0x200000, CRC(03ec3805) SHA1(a8fbaea826ca257be0b2b86952f247254929e046) ) \

// Network/Media Board firmware VER 1.19(VxWorks), 1st half contain original 1.10 version
#define SEGASP_NETFIRM \
	ROM_REGION( 0x200000, "netcpu", 0) \
	ROM_LOAD( "ic72",  0x00000000, 0x200000, CRC(a738ea1c) SHA1(d25187a973a7e166e70334f964363adf2be87257) )

// keep M4 board code happy for now
#define SEGASP_MISC \
	ROM_REGION( 0x800, "pic_readout", ROMREGION_ERASEFF ) \
	ROM_REGION(0x4, "boardid", ROMREGION_ERASEVAL(0x00)) \
	ROM_REGION( 0x84, "naomibd_eeprom", ROMREGION_ERASEFF )


ROM_START( segasp )
	SEGASP_BIOS
	SEGASP_NETFIRM
	SEGASP_MISC
ROM_END

ROM_START( brickppl )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x10000000, "rom_board", ROMREGION_ERASE)
	ROM_LOAD( "ic62",  0x00000000, 0x4000000, CRC(d79afdb6) SHA1(328e535980624d9173164b756ebbdc1ca4cb6f18) )
	ROM_LOAD( "ic63",  0x04000000, 0x4000000, CRC(4f3c0937) SHA1(72d68b66c57ff539b8058f80f1a15ffa44095460) )
	ROM_LOAD( "ic64",  0x08000000, 0x4000000, CRC(383e90d9) SHA1(eeca4b1bd0cd1fed7b85f045d71e0c7258d4350b) )
	ROM_LOAD( "ic65",  0x0c000000, 0x4000000, CRC(4c29b5ac) SHA1(9e6a79ad2d2498eed5b2590c8764222e7d6c0229) )
ROM_END

ROM_START( dinoking )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASE)
	ROM_LOAD( "ic62",  0x00000000, 0x01000000, CRC(8bd18bf7) SHA1(8972ed2bf5bc2f8af9b864118f7a22940c392079) )
	ROM_LOAD( "ic63",  0x01000000, 0x01000000, CRC(8e8c8d1b) SHA1(4ec4a91515e57524d82a0cb98beabe8a286a5cd1) )
	ROM_LOAD( "ic64",  0x02000000, 0x01000000, CRC(01b32ff7) SHA1(27301813ccd895b16a247ebed5edc3d8e3eab334) )
	ROM_LOAD( "ic65",  0x03000000, 0x01000000, CRC(4b60cdb3) SHA1(4ad3e97845d6bdd8b32369aa23e622e066c8ba67) )
	ROM_LOAD( "ic66s", 0x04000000, 0x01000000, CRC(ee3c278e) SHA1(3273a9f1eace78f65ba25bf0f6fcaa77fa421fc4) )
	ROM_LOAD( "ic67s", 0x05000000, 0x01000000, CRC(42441393) SHA1(7ba94bc12ace699ea1159cece3d070fb35789d31) )
	ROM_LOAD( "ic68s", 0x06000000, 0x01000000, CRC(4a787a44) SHA1(4d8f348466187fb67ffff8605be151cea1f77ec6) )
	ROM_LOAD( "ic69s", 0x07000000, 0x01000000, CRC(c78e46c2) SHA1(b8224c68face23010414d13ebb4cc05a2a9dce8a) )
ROM_END

ROM_START( dinokior )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASE)
	ROM_LOAD( "ic62",  0x00000000, 0x01000000, CRC(d3f37d05) SHA1(54c25ddca061acc092a357d958f180f523b86b65) )
	ROM_LOAD( "ic63",  0x01000000, 0x01000000, CRC(07a9491a) SHA1(24b4a6e2fb136dd02fd8ede3d07dc8e3cec36f3d) )
	ROM_LOAD( "ic64",  0x02000000, 0x01000000, CRC(d14a95f7) SHA1(b078ae9ca1b75a80dfd35227351b5aa6f4465cd2) )
	ROM_LOAD( "ic65",  0x03000000, 0x01000000, CRC(09bf6418) SHA1(6d833a82e1268548a837df7fb681940faee17096) )
	ROM_LOAD( "ic66s", 0x04000000, 0x01000000, CRC(014de6b8) SHA1(68c2f25bc91dee4e1e069b586314b2e1fe4bc1b2) )
	ROM_LOAD( "ic67s", 0x05000000, 0x01000000, CRC(7bf77663) SHA1(51a0c867290dce11dcc49f61c1af0d4ed42b02f1) )
	ROM_LOAD( "ic68s", 0x06000000, 0x01000000, CRC(ff5ed2b8) SHA1(d8d86b3ed976c8c8fc51d225ae661e5f237b6e1d) )
	ROM_LOAD( "ic69s", 0x07000000, 0x01000000, CRC(ab8ac4eb) SHA1(e6b3ce796ae4887011e2764261f3f437dc9939f9) )
ROM_END

ROM_START( lovebery )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASE)
	ROM_LOAD( "ic62",  0x00000000, 0x4000000, CRC(1bd80ed0) SHA1(d50307573389ebe71e381a75deb83811fa397b94) )
	ROM_LOAD( "ic63",  0x04000000, 0x4000000, CRC(d3870287) SHA1(efd3630d54068f5a8caf242a48db410bedf48e7a) )
ROM_END

ROM_START( lovebero )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASE)
	ROM_LOAD( "ic62",  0x00000000, 0x4000000, CRC(0a23cea3) SHA1(1780d935b0d641769859b2022df8e4262e7bafd8) )
	ROM_LOAD( "ic63",  0x04000000, 0x4000000, CRC(d3870287) SHA1(efd3630d54068f5a8caf242a48db410bedf48e7a) )
ROM_END

ROM_START( tetgiant )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASE)
	ROM_LOAD( "ic62",  0x00000000, 0x4000000, CRC(31ba1938) SHA1(9b5a05193b3df13cd7617a38913e0b0fbd61da44) )
	ROM_LOAD( "ic63",  0x04000000, 0x4000000, CRC(cb946213) SHA1(6195e33c44a1e8eb464dfc3558dc1c9b4d910ef3) )
ROM_END


ROM_START( dinoki25 )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASEFF)

	DISK_REGION( "cflash" )
	DISK_IMAGE( "mda-c0047", 0, SHA1(0f97291d9c5dbe3e66a5220da05aebdfaa78b35d) )
ROM_END

ROM_START( loveber3 )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASEFF)

	DISK_REGION( "cflash" )
	DISK_IMAGE( "mda-c0042", 0, SHA1(9992d90dae8ce7636e4153e02b779c27931b3be6) )
ROM_END

ROM_START( tetgiano )
	SEGASP_BIOS
	SEGASP_MISC

	ROM_REGION( 0x08000000, "rom_board", ROMREGION_ERASEFF)

	DISK_REGION( "cflash" )
	DISK_IMAGE( "mda-c0076", 0, SHA1(6987c888d2a3ada2d07f6396d47fdba507ca859d) )
ROM_END


#define GAME_FLAGS (GAME_NO_SOUND|GAME_NOT_WORKING)

GAME( 2004, segasp,  0,          segasp,    segasp, driver_device,    0, ROT0, "Sega", "Sega System SP (Spider) BIOS", GAME_FLAGS | GAME_IS_BIOS_ROOT )

// These use ROMs
GAME( 2009, brickppl,segasp,     segasp,    segasp, driver_device,    0, ROT0, "Sega", "Brick People / Block PeePoo (Ver 1.002)", GAME_FLAGS )

GAME( 2005, dinoking,segasp,     segasp,    segasp, driver_device,    0, ROT0, "Sega", "Dinosaur King (USA)", GAME_FLAGS )

GAME( 2006, dinokior,segasp,     segasp,    segasp, driver_device,    0, ROT0, "Sega", "Dinosaur King - Operation: Dinosaur Rescue (USA, Export)", GAME_FLAGS )

GAME( 2006, lovebery,segasp,     segasp,    segasp, driver_device,    0, ROT0, "Sega", "Love And Berry - 1st-2nd Collection (Export, Ver 2.000)", GAME_FLAGS )
GAME( 2006, lovebero,lovebery,   segasp,    segasp, driver_device,    0, ROT0, "Sega", "Love And Berry - 1st-2nd Collection (Export, Ver 1.003)", GAME_FLAGS )

GAME( 2009, tetgiant,segasp,     segasp,    segasp, driver_device,    0, ROT0, "Sega", "Tetris Giant / Tetris Dekaris (Ver.2.000)", GAME_FLAGS )

// These use a CF card

GAME( 2008, dinoki25,segasp,     segasp,    segasp, driver_device,    0, ROT0, "Sega", "Dinosaur King - D-Team VS. the Alpha Fortress (Export, Ver 2.500) (MDA-C0047)", GAME_FLAGS )

GAME( 2007, loveber3,segasp,     segasp,    segasp, driver_device,    0, ROT0, "Sega", "Love And Berry - 3rd-5th Collection (USA, Export, Ver 1.002) (MDA-C0042)", GAME_FLAGS )

GAME( 2009, tetgiano,tetgiant,   segasp,    segasp, driver_device,    0, ROT0, "Sega", "Tetris Giant / Tetris Dekaris (MDA-C0076)", GAME_FLAGS )
