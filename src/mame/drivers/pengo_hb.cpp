// license:BSD-3-Clause
// copyright-holders:Robbbert

/* The colours for alpacap7 and alpacap8 are completely wrong, but we are using the roms specified by
	the original programmer. See http://umlautllama.com/projects/alpaca */
ROM_START( alpacap7 )
	ROM_REGION( 0x10000, "maincpu", 0 )	/* 64k for code */
	ROM_LOAD( "alp7.u8",		0x0000, 0x1000, CRC(E70923E4) SHA1(86A96DC0DEE5F5B532121892528ED2B456D51174) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "alp7_ic92",		0x0000, 0x1000, CRC(693B1A96) SHA1(DED8AC0A4186458FDA6A241C027718742BA87973) )
	ROM_CONTINUE(             0x2000, 0x1000 ) /* sprites (bank 1) */
	ROM_LOAD( "alp_ic105",		0x1000, 0x1000, CRC(BDA193F4) SHA1(A61B5E86809FEFF025C73DE78FC751EAD646125B) )
	ROM_CONTINUE(             0x3000, 0x1000 ) /* sprites (bank 1) */

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) ) /* color palette */
	ROM_LOAD( "pr1634.88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) ) /* color lookup */

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) ) /* waveform */
	ROM_LOAD( "pr1636.70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) ) /* timing - not used */
ROM_END

ROM_START( alpacap8 )
	ROM_REGION( 0x10000, "maincpu", 0 )	/* 64k for code */
	ROM_LOAD( "alp8.u8",		0x0000, 0x1000, CRC(ABD45FD7) SHA1(120276D1E9A3DA707DB4263A93418CA37F4F4C9F) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "alp8_ic92",		0x0000, 0x1000, CRC(4A45717F) SHA1(159A74C31FC5D4E5405BCD8D8843A1B381EA01A4) )
	ROM_CONTINUE(             0x2000, 0x1000 ) /* sprites (bank 1) */
	ROM_LOAD( "alp_ic105",		0x1000, 0x1000, CRC(BDA193F4) SHA1(A61B5E86809FEFF025C73DE78FC751EAD646125B) )
	ROM_CONTINUE(             0x3000, 0x1000 ) /* sprites (bank 1) */

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) ) /* color palette */
	ROM_LOAD( "pr1634.88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) ) /* color lookup */

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) ) /* waveform */
	ROM_LOAD( "pr1636.70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) ) /* timing - not used */
ROM_END

ROM_START( pengopop )
	ROM_REGION( 0x10000, "maincpu", 0 ) /* 64k for code */
	ROM_LOAD( "pengo.u8",     0x0000, 0x1000, CRC(3dfeb20e) SHA1(a387b72501da77bf38b58619d2099083a0463e1f) )
	ROM_LOAD( "pengo.u7",     0x1000, 0x1000, CRC(1db341bd) SHA1(d1c66bb9cf479e6960dbcd35c820097a81eaa555) )
	ROM_LOAD( "pengo.u15",    0x2000, 0x1000, CRC(7c2842d5) SHA1(a8a568da68babd0ccb9f2cee4182fc01c3138494) )
	ROM_LOAD( "pengo.u14",    0x3000, 0x1000, CRC(6e3c1f2f) SHA1(2ee821b0f6e0f3cfeae7f5ff25a6e9bd977efce0) )
	ROM_LOAD( "ep5124.21",    0x4000, 0x1000, CRC(95f354ff) SHA1(fdebc68a6d87f8ecdf52a57a34ae5ae844a13510) )
	ROM_LOAD( "pengo.u20",    0x5000, 0x1000, CRC(0fdb04b8) SHA1(ed814d58318c1055e475ff678609d189727bf9b4) )
	ROM_LOAD( "ep5126.32",    0x6000, 0x1000, CRC(e5920728) SHA1(0ac5ffdad7bdcb32e630b9582e1b1aaece5198c9) )
	ROM_LOAD( "pengopc.u31",  0x7000, 0x1000, CRC(1ede8569) SHA1(0d10a0896847a06185a91eb83c0ccb88c4307b33) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "ep1640.92",    0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) ) /* tiles (bank 1) */
	ROM_CONTINUE(             0x2000, 0x1000 ) /* sprites (bank 1) */
	ROM_LOAD( "ep1695.105",   0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) ) /* tiles (bank 2) */
	ROM_CONTINUE(             0x3000, 0x1000 ) /* sprites (bank 2) */

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) ) /* color palette */
	ROM_LOAD( "pr1634.88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) ) /* color lookup */

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) ) /* waveform */
	ROM_LOAD( "pr1636.70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) ) /* timing - not used */
ROM_END

ROM_START( vecpengo )
	ROM_REGION( 2*0x10000, "maincpu", 0 )     /* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "ep1689c.8",    0x0000, 0x1000, CRC(f37066a8) SHA1(0930de17a763a527057f60783a92662b09554426) )
	ROM_LOAD( "ep1690b.7",    0x1000, 0x1000, CRC(baf48143) SHA1(4c97529e61eeca5d94938b1dfbeac41bf8cbaf7d) )
	ROM_LOAD( "ep1691b.15",   0x2000, 0x1000, CRC(adf0eba0) SHA1(c8949fbdbfe5023ee17a789ef60205e834a76c81) )
	ROM_LOAD( "ep1692b.14",   0x3000, 0x1000, CRC(a086d60f) SHA1(7079769d14dfe3873ffe29623ba0a93413706c6d) )
	ROM_LOAD( "ep1693b.21",   0x4000, 0x1000, CRC(b72084ec) SHA1(c0508951c2ad8dc31481be8b3bfee2063e3fb0d7) )
	ROM_LOAD( "ep1694b.20",   0x5000, 0x1000, CRC(94194a89) SHA1(7b47aec61593efd758e2a031f72a854bb0ba8af1) )
	ROM_LOAD( "ep5118b.32",   0x6000, 0x1000, CRC(af7b12c4) SHA1(207ed466546f40ca60a38031b83aef61446902e2) )
	ROM_LOAD( "ep5119c.31",   0x7000, 0x1000, CRC(933950fe) SHA1(fec7236b3dee2ea6e39c68440a6d2d9e3f72675a) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "vecp_ic92",    0x0000, 0x1000, CRC(57c5e53c) SHA1(4d1d4cdc352cb2fd14ebbd6678211093be73fb69) ) /* tiles (bank 1) */
	ROM_CONTINUE(         	  0x2000, 0x1000 ) /* sprites (bank 1) */
	ROM_LOAD( "vecp_ic105",   0x1000, 0x1000, CRC(b93588b0) SHA1(bbb779e538bdf7ebfcb0e12e11b57cabd5ddd29d) ) /* tiles (bank 2) */
	ROM_CONTINUE(             0x3000, 0x1000 ) /* sprites (bank 2) */

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.78",	  0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) ) /* color palette */
	ROM_LOAD( "pr1634.88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) ) /* color lookup */

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) ) /* waveform */
	ROM_LOAD( "pr1636.70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) ) /* timing - not used */
ROM_END


GAME( 2003, alpacap7, alpaca8, pengou, pengo, driver_device, 0,     ROT90, "Scott Lawrence", "Alpaca v0.7 (Pengo Hardware)", MACHINE_IMPERFECT_COLORS )
GAME( 2003, alpacap8, alpaca8, pengou, pengo, driver_device, 0,     ROT90, "Scott Lawrence", "Alpaca v0.8 (Pengo Hardware)", MACHINE_IMPERFECT_COLORS )
GAME( 1997, pengopop, pengo,   pengou, pengo, driver_device, 0,     ROT90, "Sega", "Pengo (Popcorn Music)", 0 )
GAME( 2000, vecpengo, pengo,   pengo,  pengo, driver_device, 0,     ROT90, "T-Bone", "Pengo (Vector sim)", 0 )
