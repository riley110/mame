/***************************************************************************
  Rainbow Islands

  A lot of this is based on Rastan driver, it may be re-joined with
  that at a later date.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *rastan_ram;
extern unsigned char *rastan_videoram1,*rastan_videoram3;
extern int rastan_videoram_size;
extern unsigned char *rastan_spriteram;
extern unsigned char *rastan_scrollx;
extern unsigned char *rastan_scrolly;

void rastan_spriteram_w(int offset,int data);
int rastan_spriteram_r(int offset);
void rastan_videoram1_w(int offset,int data);
int rastan_videoram1_r(int offset);
void rastan_videoram3_w(int offset,int data);
int rastan_videoram3_r(int offset);

void rastan_scrollY_w(int offset,int data);
void rastan_scrollX_w(int offset,int data);

void rastan_videocontrol_w(int offset,int data);

int rastan_interrupt(void);
int rastan_s_interrupt(void);

int rastan_input_r (int offset);

void rastan_sound_w(int offset,int data);
int rastan_sound_r(int offset);

void rastan_speedup_w(int offset,int data);
int rastan_speedup_r(int offset);


int r_rd_a000(int offset);
int r_rd_a001(int offset);
void r_wr_a000(int offset,int data);
void r_wr_a001(int offset,int data);

int  rastan_sh_init(const char *gamename);
void r_wr_b000(int offset,int data);
void r_wr_c000(int offset,int data);
void r_wr_d000(int offset,int data);

/* Rainbow Extras */

int  rainbow_interrupt(void);
void rainbow_c_chip_w(int offset, int data);
int  rainbow_c_chip_r(int offset);
void rainbow_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

int  rastan_vh_start(void);
void rastan_vh_stop(void);

/* Jumping Extras */

void jumping_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static struct MemoryReadAddress rainbow_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x10c000, 0x10ffff, MRA_BANK1 },	/* RAM */
	{ 0x200000, 0x20ffff, paletteram_word_r },
    { 0x390000, 0x390003, input_port_0_r },
    { 0x3B0000, 0x3B0003, input_port_1_r },
	{ 0x3e0000, 0x3e0003, rastan_sound_r },
    { 0x800000, 0x80ffff, rainbow_c_chip_r },
	{ 0xc00000, 0xc03fff, rastan_videoram1_r },
	{ 0xc04000, 0xc07fff, MRA_BANK2 },
	{ 0xc08000, 0xc0bfff, rastan_videoram3_r },
	{ 0xc0c000, 0xc0ffff, MRA_BANK3 },
	{ 0xd00000, 0xd0ffff, MRA_BANK4, &rastan_spriteram },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress rainbow_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x10c000, 0x10ffff, MWA_BANK1 },
	{ 0x200000, 0x20ffff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram },
    { 0x800000, 0x80ffff, rainbow_c_chip_w },
	{ 0xc00000, 0xc03fff, rastan_videoram1_w, &rastan_videoram1, &rastan_videoram_size },
	{ 0xc04000, 0xc07fff, MWA_BANK2 },
	{ 0xc08000, 0xc0bfff, rastan_videoram3_w, &rastan_videoram3 },
	{ 0xc0c000, 0xc0ffff, MWA_BANK3 },
	{ 0xc20000, 0xc20003, rastan_scrollY_w, &rastan_scrolly },  /* scroll Y  1st.w plane1  2nd.w plane2 */
	{ 0xc40000, 0xc40003, rastan_scrollX_w, &rastan_scrollx },  /* scroll X  1st.w plane1  2nd.w plane2 */
	{ 0xd00000, 0xd0ffff, MWA_BANK4 },
	{ 0x380000, 0x380003, rastan_videocontrol_w },	/* sprite palette bank, coin counters, other unknowns */
	{ 0x3e0000, 0x3e0003, rastan_sound_w },
	{ 0x3c0000, 0x3c0003, MWA_NOP },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress jumping_readmem[] =
{
	{ 0x000000, 0x08ffff, MRA_ROM },
	{ 0x10c000, 0x10ffff, MRA_BANK1 },	/* RAM */
	{ 0x200000, 0x20ffff, paletteram_word_r },
    { 0x400000, 0x400001, input_port_0_r },
    { 0x400002, 0x400003, input_port_1_r },
	{ 0x400006, 0x400007, rastan_sound_r },			/* What Chip ? */
    { 0x401000, 0x401001, input_port_3_r },
    { 0x401002, 0x401003, input_port_4_r },
	{ 0xc00000, 0xc03fff, rastan_videoram1_r },
	{ 0xc04000, 0xc07fff, MRA_BANK2 },
	{ 0xc08000, 0xc0bfff, rastan_videoram3_r },
	{ 0xc0c000, 0xc0ffff, MRA_BANK3 },
	{ 0x440000, 0x4407ff, MRA_BANK4, &rastan_spriteram },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress jumping_writemem[] =
{
	{ 0x000000, 0x08ffff, MWA_ROM },
	{ 0x10c000, 0x10ffff, MWA_BANK1 },
	{ 0x200000, 0x20ffff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0xc00000, 0xc03fff, rastan_videoram1_w, &rastan_videoram1, &rastan_videoram_size },
	{ 0xc04000, 0xc07fff, MWA_BANK2 },
	{ 0xc08000, 0xc0bfff, rastan_videoram3_w, &rastan_videoram3 },
	{ 0xc0c000, 0xc0ffff, MWA_BANK3 },
    { 0x430000, 0x430003, rastan_scrollY_w, &rastan_scrolly },  /* scroll Y  1st.w plane1  2nd.w plane2 */
   	{ 0xc40000, 0xc40003, rastan_scrollX_w, &rastan_scrollx },  /* scroll X  1st.w plane1  2nd.w plane2 */
    { 0x440000, 0x4407ff, MWA_BANK4 },
	{ 0x3e0000, 0x3e0003, rastan_sound_w },
	{ -1 }  /* end of table */
};


#if 0
static struct MemoryReadAddress rastan_s_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8fff, MRA_RAM },
	{ 0x9001, 0x9001, YM2151_read_port_0_r },
	{ 0x9002, 0x9100, MRA_RAM },
	{ 0xa000, 0xa000, r_rd_a000 },
	{ 0xa001, 0xa001, r_rd_a001 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress rastan_s_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8fff, MWA_RAM },
	{ 0x9000, 0x9000, YM2151_control_port_0_w },
	{ 0x9001, 0x9001, YM2151_write_port_0_w },
	{ 0xa000, 0xa000, r_wr_a000 },
	{ 0xa001, 0xa001, r_wr_a001 },
	{ 0xb000, 0xb000, r_wr_b000 },
	{ 0xc000, 0xc000, r_wr_c000 },
	{ 0xd000, 0xd000, r_wr_d000 },
	{ -1 }  /* end of table */
};
#endif



INPUT_PORTS_START( rainbow_input_ports )
	PORT_START	/* DIP SWITCH A */
	PORT_DIPNAME( 0x01, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x01, "Cocktail" )
	PORT_DIPNAME( 0x02, 0x02, "Flip Screen", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_BITX(    0x04, 0x04, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x04, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x08, 0x08, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x08, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x30, 0x30, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x10, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x20, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit" )
	PORT_DIPNAME( 0xc0, 0xc0, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0xc0, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x80, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x40, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x00, "1 Coin/6 Credits" )

	PORT_START	/* DIP SWITCH B */
	PORT_DIPNAME( 0x03, 0x03, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x00, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "100k,1000k" )
	PORT_DIPSETTING(    0x04, "None" )
	PORT_DIPNAME( 0x08, 0x00, "Complete Bonus", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "1 Up" )
	PORT_DIPSETTING(    0x08, "None" )
	PORT_DIPNAME( 0x30, 0x30, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPNAME( 0x40, 0x00, "Language", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "Japanese" )
	PORT_DIPNAME( 0x80, 0x00, "Coin Type", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Type 1" )
	PORT_DIPSETTING(    0x80, "Type 2" )

	PORT_START	/* 800007 */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE )

    PORT_START /* 800009 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )

	PORT_START	/* 80000B */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON2 )

	PORT_START	/* 80000d */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( jumping_input_ports )
	PORT_START	/* DIP SWITCH A */
	PORT_DIPNAME( 0x01, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x01, "Cocktail" )
	PORT_DIPNAME( 0x02, 0x02, "Flip Screen", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_BITX(    0x04, 0x04, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x04, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x08, 0x08, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x08, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x30, 0x30, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x10, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x20, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit" )
	PORT_DIPNAME( 0xc0, 0xc0, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0xc0, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x80, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x40, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x00, "1 Coin/6 Credits" )

	PORT_START	/* DIP SWITCH B */
	PORT_DIPNAME( 0x03, 0x03, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x00, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "100k,1000k" )
	PORT_DIPSETTING(    0x04, "None" )
	PORT_DIPNAME( 0x08, 0x00, "Complete Bonus", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "1 Up" )
	PORT_DIPSETTING(    0x08, "None" )
	PORT_DIPNAME( 0x30, 0x30, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPNAME( 0x40, 0x00, "Language", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "Japanese" )
	PORT_DIPNAME( 0x80, 0x00, "Coin Type", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Type 1" )
	PORT_DIPSETTING(    0x80, "Type 2" )

	PORT_START	/* Probably not used! */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE )

    PORT_START  /* 401001 - Coins Etc. */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )						/* OK */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )              		/* OK */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )             		/* OK */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )             		/* OK */

	PORT_START	/* 401003 - Player Controls */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )            		/* OK */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )            		/* OK */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY )	/* OK */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY )   /* OK */

	PORT_START	/* Probably not used */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )

	PORT_START	/* Probably not used */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END


static struct GfxLayout spritelayout1 =
{
	8,8,	/* 8*8 sprites */
	16384,	/* 16384 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
    { 8, 12, 0, 4, 24, 28, 16, 20 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every sprite takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout2 =
{
	16,16,	/* 16*16 sprites */
	4096,	/* 1024 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 8, 12, 0, 4, 24, 28, 16, 20, 40, 44, 32, 36, 56, 60, 48, 52 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout spritelayout3 =
{
	16,16,	/* 16*16 sprites */
	1024,	/* 1024 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{
	0, 4, 0x10000*8+0 ,0x10000*8+4,
	8+0, 8+4, 0x10000*8+8+0, 0x10000*8+8+4,
	16+0, 16+4, 0x10000*8+16+0, 0x10000*8+16+4,
	24+0, 24+4, 0x10000*8+24+0, 0x10000*8+24+4
	},
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
			8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxDecodeInfo rainbowe_gfxdecodeinfo[] =
{
	{ 1, 0x000000, &spritelayout1, 0, 0x80 },	/* sprites 8x8 */
	{ 1, 0x080000, &spritelayout2, 0, 0x80 },	/* sprites 16x16 */
	{ 1, 0x100000, &spritelayout3, 0, 0x80 },/* sprites 16x16 (What For ?) */
	{ -1 } 										/* end of array */
};


/* There are 5120 sprites, as indeed there are in Rainbow Island */
/* However, it looks like the hardware will only ever select up  */
/* to 4096 - so what are the others for ?                        */
/*                                                               */
/* Incidentally, sprite roms need all bits reversing, as colours */
/* are mapped back to front from the pattern used by Rainbow!    */

static struct GfxLayout jumping_tilelayout =
{
	8,8,	/* 8*8 sprites */
	16384,	/* 16384 sprites */
	4,		/* 4 bits per pixel */
	{ 0, 0x20000*8, 0x40000*8, 0x60000*8 },
    { 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8		/* every sprite takes 8 consecutive bytes */
};

static struct GfxLayout jumping_spritelayout =
{
	16,16,	/* 16*16 sprites */
	5120,	/* 5120 sprites */
	4,	/* 4 bits per pixel */
	{ 0x78000*8,0x50000*8,0x28000*8,0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8*16+0, 8*16+1, 8*16+2, 8*16+3, 8*16+4, 8*16+5, 8*16+6, 8*16+7 },
	{ 0, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo jumping_gfxdecodeinfo[] =
{
	{ 1, 0x000000, &jumping_tilelayout, 0, 0x80 },	/* sprites 8x8 */
	{ 1, 0x080000, &jumping_spritelayout, 0, 0x80 },	/* sprites 16x16 */
	{ -1 } 										/* end of array */
};

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,	/* 8 Mhz */
			0,
			rainbow_readmem,rainbow_writemem,0,0,
			rainbow_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slices per frame - no sound CPU yet! */
	0,

	/* video hardware */
	40*8, 32*8, { 0*8, 40*8-1, 1*8, 31*8-1 },
	rainbowe_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	rastan_vh_start,
	rastan_vh_stop,
	rainbow_vh_screenrefresh,

	/* sound hardware */
    0,
    0,
    0,
    0
};

static struct MachineDriver jumping_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,	/* 8 Mhz */
			0,
			jumping_readmem,jumping_writemem,0,0,
			rainbow_interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slices per frame - no sound CPU yet! */
	0,

	/* video hardware */
	40*8, 32*8, { 0*8, 40*8-1, 1*8, 31*8-1 },
	jumping_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	rastan_vh_start,
	rastan_vh_stop,
	jumping_vh_screenrefresh,

	/* sound hardware */
    0,
    0,
    0,
    0
};



ROM_START( rainbow_rom )
	ROM_REGION(0x80000)		/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "b22-10",       0x00000, 0x10000, 0x3b013495 )
	ROM_LOAD_ODD ( "b22-11",       0x00000, 0x10000, 0x80041a3d )
	ROM_LOAD_EVEN( "b22-08",       0x20000, 0x10000, 0x962fb845 )
	ROM_LOAD_ODD ( "b22-09",       0x20000, 0x10000, 0xf43efa27 )
	ROM_LOAD_EVEN( "ri_m03.rom",   0x40000, 0x20000, 0x3ebb0fb8 )
	ROM_LOAD_ODD ( "ri_m04.rom",   0x40000, 0x20000, 0x91625e7f )

	ROM_REGION_DISPOSE(0x120000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ri_m01.rom",   0x000000, 0x80000, 0xb76c9168 )        /* 8x8 gfx */
  	ROM_LOAD( "ri_m02.rom",   0x080000, 0x80000, 0x1b87ecf0 )        /* sprites */
	ROM_LOAD( "b22-13",       0x100000, 0x10000, 0x2fda099f )
	ROM_LOAD( "b22-12",       0x110000, 0x10000, 0x67a76dc6 )

#if 0
	ROM_REGION(0x10000)		/* 64k for the audio CPU */
	ROM_LOAD( "b22-14",       0x0000, 0x10000, 0x0 )
#endif
ROM_END

ROM_START( rainbowe_rom )
	ROM_REGION(0x80000)		/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "ri_01.rom",    0x00000, 0x10000, 0x50690880 )
	ROM_LOAD_ODD ( "ri_02.rom",    0x00000, 0x10000, 0x4dead71f )
	ROM_LOAD_EVEN( "ri_03.rom",    0x20000, 0x10000, 0x4a4cb785 )
	ROM_LOAD_ODD ( "ri_04.rom",    0x20000, 0x10000, 0x4caa53bd )
	ROM_LOAD_EVEN( "ri_m03.rom",   0x40000, 0x20000, 0x3ebb0fb8 )
	ROM_LOAD_ODD ( "ri_m04.rom",   0x40000, 0x20000, 0x91625e7f )

	ROM_REGION_DISPOSE(0x120000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ri_m01.rom",   0x000000, 0x80000, 0xb76c9168 )        /* 8x8 gfx */
  	ROM_LOAD( "ri_m02.rom",   0x080000, 0x80000, 0x1b87ecf0 )        /* sprites */
	ROM_LOAD( "b22-13",       0x100000, 0x10000, 0x2fda099f )
	ROM_LOAD( "b22-12",       0x110000, 0x10000, 0x67a76dc6 )

#if 0
	ROM_REGION(0x10000)		/* 64k for the audio CPU */
	ROM_LOAD( "b22-14",       0x0000, 0x10000, 0x0 )
#endif
ROM_END

static void jumping_sprite_decode(void)
{
	/* Sprite colour map is reversed - switch to normal */

	int Count;

    for(Count=0x118000;Count>=0x80000;Count--) Machine->memory_region[1][Count]^=0xFF;
}

ROM_START( jumping_rom )
	ROM_REGION(0xA0000)		/* 8*64k for code, 64k*2 for protection chip */
    ROM_LOAD_EVEN( "jb1_h4",       0x00000, 0x10000, 0x3fab6b31 )
    ROM_LOAD_ODD ( "jb1_h8",       0x00000, 0x10000, 0x8c878827 )
    ROM_LOAD_EVEN( "jb1_i4",       0x20000, 0x10000, 0x443492cf )
    ROM_LOAD_ODD ( "jb1_i8",       0x20000, 0x10000, 0xed33bae1 )
	ROM_LOAD_EVEN( "ri_m03.rom",   0x40000, 0x20000, 0x3ebb0fb8 )
	ROM_LOAD_ODD ( "ri_m04.rom",   0x40000, 0x20000, 0x91625e7f )
    ROM_LOAD_ODD ( "jb1_f89",      0x80000, 0x10000, 0x0810d327 ) 	/* Dump of C-Chip? */

    ROM_REGION_DISPOSE(0x120000)    /* Graphics */
    ROM_LOAD( "jb2_ic8",      0x00000, 0x10000, 0x65b76309 )			/* 8x8 characters */
    ROM_LOAD( "jb2_ic7",      0x10000, 0x10000, 0x43a94283 )
    ROM_LOAD( "jb2_ic10",     0x20000, 0x10000, 0xe61933fb )
    ROM_LOAD( "jb2_ic9",      0x30000, 0x10000, 0xed031eb2 )
    ROM_LOAD( "jb2_ic12",     0x40000, 0x10000, 0x312700ca )
    ROM_LOAD( "jb2_ic11",     0x50000, 0x10000, 0xde3b0b88 )
    ROM_LOAD( "jb2_ic14",     0x60000, 0x10000, 0x9fdc6c8e )
    ROM_LOAD( "jb2_ic13",     0x70000, 0x10000, 0x06226492 )

    ROM_LOAD( "jb2_ic62",     0x80000, 0x10000, 0x8548db6c )			/* 16x16 sprites */
    ROM_LOAD( "jb2_ic61",     0x90000, 0x10000, 0x37c5923b )
    ROM_LOAD( "jb2_ic60",     0xA0000, 0x08000, 0x662a2f1e )
    ROM_LOAD( "jb2_ic78",     0xA8000, 0x10000, 0x925865e1 )
    ROM_LOAD( "jb2_ic77",     0xB8000, 0x10000, 0xb09695d1 )
    ROM_LOAD( "jb2_ic76",     0xC8000, 0x08000, 0x41937743 )
    ROM_LOAD( "jb2_ic93",     0xD0000, 0x10000, 0xf644eeab )
    ROM_LOAD( "jb2_ic92",     0xE0000, 0x10000, 0x3fbccd33 )
    ROM_LOAD( "jb2_ic91",     0xF0000, 0x08000, 0xd886c014 )
    ROM_LOAD( "jb2_i121",     0xF8000, 0x10000, 0x93df1e4d )
    ROM_LOAD( "jb2_i120",     0x108000, 0x10000, 0x7c4e893b )
    ROM_LOAD( "jb2_i119",     0x118000, 0x08000, 0x7e1d58d8 )

#if 0
	ROM_REGION(0x10000)		/* 64k for the audio CPU */
	ROM_LOAD( "jb1_cd67",     0x0000, 0x10000, 0x0 )
#endif

ROM_END


struct GameDriver rainbow_driver =
{
	__FILE__,
	0,
	"rainbow",
	"Rainbow Islands",
	"1987",
	"Taito",
	"Richard Bush (Raine & Info)\nMike Coates (MAME driver)",
	GAME_NOT_WORKING,
	&machine_driver,
	0,

	rainbow_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	rainbow_input_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	0, 0
};

struct GameDriver rainbowe_driver =
{
	__FILE__,
	&rainbow_driver,
	"rainbowe",
	"Rainbow Islands (Extra)",
	"1988",
	"Taito",
	"Richard Bush (Raine & Info)\nMike Coates (MAME driver)",
	GAME_NOT_WORKING,
	&machine_driver,
	0,

	rainbowe_rom,
	0, 0,
	0,
	0,	/* sound_prom */

	rainbow_input_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	0, 0
};

struct GameDriver jumping_driver =
{
	__FILE__,
	&rainbow_driver,
	"jumping",
	"Jumping",
	"1989",
	"bootleg",
	"Richard Bush (Raine & Info)\nMike Coates (MAME driver)",
	0,
	&jumping_machine_driver,
	0,

	jumping_rom,
	jumping_sprite_decode, 0,
	0,
	0,	/* sound_prom */

	jumping_input_ports,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	0, 0
};
