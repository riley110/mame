/***************************************************************************
	Nemesis preliminary :

	0x00000 - 0x3ffff       ROM

	0x40000 - 0x4ffff	character RAM + Obj ram

	scroll value is the same for every line in nemesis
	0x50000 - 0x501ff	scroll index per line layer 1 (low byte)
	0x50200 - 0x503ff	scroll index per line layer 1 (hi byte)
	0x50400 - 0x505ff	scroll index per line layer 2 (low byte)
	0x50600 - 0x507ff	scroll index per line layer 2 (hi byte)

	0x50800 - 0x50fff	??????

	0x52000 - 0x52fff       screen RAM 1
	0x53000 - 0x53fff       screen RAM 2

	0x54000 - 0x54fff	color ram 1
	0x55000 - 0x55fff	color ram 2

	0x56000 - 0x56fff       sprite RAM

	0x5c401
	0x5c403

	0x5cc01
	0x5cc03

	0x5cc05
	0x5cc07

	0x5c801			(watchdog ???)

	0x5a000 - 0x5afff       pallette RAM

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char *nemesis_videoram1;
extern unsigned char *nemesis_videoram2;
extern unsigned char *nemesis_characterram;
extern unsigned char *nemesis_paletteram;
extern unsigned char *nemesis_xscroll1,*nemesis_xscroll2, *nemesis_yscroll;

int  nemesis_videoram1_r(int offset);
void nemesis_videoram1_w(int offset,int data);
int  nemesis_videoram2_r(int offset);
void nemesis_videoram2_w(int offset,int data);
int  nemesis_paletteram_r(int offset);
void nemesis_paletteram_w(int offset,int data);
int  nemesis_characterram_r(int offset);
void nemesis_characterram_w(int offset,int data);
void nemesis_vh_screenrefresh(struct osd_bitmap *bitmap);
int  nemesis_vh_start(void);
void nemesis_vh_stop(void);

void salamand_vh_screenrefresh(struct osd_bitmap *bitmap);



int irq_on = 0;
int nemesis_interrupt(void)
{
	if (irq_on)
		return(1);
	else
		return(0);
}

int salamand_interrupt(void)
{
	if (irq_on)
		return(1);
	else
		return(0);
}

void nemesis_irq_enable_w(int offset,int data)
{
	irq_on = data & 0xff;
}

void nemesis_soundlatch_w (int offset, int data)
{
	soundlatch_w(offset,data & 0xff);

	/* the IRQ should probably be generated by 5e004, but we'll handle it here for now */
	cpu_cause_interrupt(1,0xff);
}

static int nemesis_portA_r(int offset)
{
	#define TIMER_RATE 1024

	return cpu_gettotalcycles() / TIMER_RATE;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0x040000, 0x04ffff, nemesis_characterram_r },
	{ 0x050000, 0x0503ff, MRA_BANK1 },
	{ 0x050400, 0x0507ff, MRA_BANK2 },
	{ 0x050800, 0x050bff, MRA_BANK3 },
	{ 0x050c00, 0x050fff, MRA_BANK4 },

	{ 0x052000, 0x053fff, nemesis_videoram1_r },
	{ 0x054000, 0x055fff, nemesis_videoram2_r },
	{ 0x056000, 0x056fff, MRA_BANK5 },
	{ 0x05a000, 0x05afff, nemesis_paletteram_r },

	{ 0x05c400, 0x05c401, input_port_4_r },	/* DSW0 */
	{ 0x05c402, 0x05c403, input_port_5_r },	/* DSW1 */

	{ 0x05cc00, 0x05cc01, input_port_0_r },	/* IN0 */
	{ 0x05cc02, 0x05cc03, input_port_1_r },	/* IN1 */
	{ 0x05cc04, 0x05cc05, input_port_2_r },	/* IN2 */
	{ 0x05cc06, 0x05cc07, input_port_3_r },	/* TEST */

	{ 0x060000, 0x067fff, MRA_BANK6 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },	/* ROM */

	{ 0x040000, 0x04ffff, nemesis_characterram_w, &nemesis_characterram },

	{ 0x050000, 0x0503ff, MWA_BANK1, &nemesis_xscroll1 },
	{ 0x050400, 0x0507ff, MWA_BANK2, &nemesis_xscroll2 },
	{ 0x050800, 0x050bff, MWA_BANK3 },
	{ 0x050c00, 0x050fff, MWA_BANK4, &nemesis_yscroll },

	{ 0x052000, 0x053fff, nemesis_videoram1_w, &nemesis_videoram1 },	/* VRAM 1 */
	{ 0x054000, 0x055fff, nemesis_videoram2_w, &nemesis_videoram2 },	/* VRAM 2 */
	{ 0x056000, 0x056fff, MWA_BANK5, &spriteram, &spriteram_size },
	{ 0x05a000, 0x05afff, nemesis_paletteram_w, &nemesis_paletteram },

	{ 0x05c000, 0x05c001, nemesis_soundlatch_w },
	{ 0x05c800, 0x05c801, watchdog_reset_w },	/* probably */

	{ 0x05e000, 0x05e001, &nemesis_irq_enable_w },	/* Nemesis */
	{ 0x05e002, 0x05e003, &nemesis_irq_enable_w },	/* Konami GT */
/*	{ 0x05e004, 0x05e005, },	bit 8 of the word probably triggers IRQ on sound board */

	{ 0x060000, 0x067fff, MWA_BANK6 },	/* WORK RAM */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x47ff, MRA_RAM },
	{ 0xe001, 0xe001, soundlatch_r },
	{ 0xe086, 0xe086, AY8910_read_port_0_r },
	{ 0xe205, 0xe205, AY8910_read_port_1_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x47ff, MWA_RAM },
	{ 0xe006, 0xe006, AY8910_control_port_0_w },
	{ 0xe106, 0xe106, AY8910_write_port_0_w },
	{ 0xe005, 0xe005, AY8910_control_port_1_w },
	{ 0xe405, 0xe405, AY8910_write_port_1_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress salamand_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },  /* ROM BIOS */
	{ 0x040000, 0x07ffff, MRA_ROM },  /* not all banks filled */
	{ 0x080000, 0x087fff, MRA_RAM },
	{ 0x120000, 0x12ffff, MRA_RAM },
	{ 0x190000, 0x190FFF, MRA_RAM },
	{ 0x100000, 0x101FFF, MRA_RAM },
	{ 0x102000, 0x103FFF, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress salamand_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0x080000, 0x087fff, MWA_RAM },
	{ 0x090000, 0x09ffff, MWA_RAM },
	{ 0x0A0000, 0x0A0001, nemesis_irq_enable_w },          /* irq enable */
	{ 0x0C0004, 0x0C0007, MWA_NOP },        /* Watchdog at $c0005 */
	{ 0x120000, 0x12ffff, MWA_RAM },
	{ 0x180000, 0x180FFF, MWA_RAM },
	{ 0x190000, 0x190FFF, MWA_RAM },

	{ 0x100000, 0x101FFF, MWA_RAM },
	{ 0x102000, 0x103FFF, MWA_RAM },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( nemesis_input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* TEST */
	PORT_DIPNAME( 0x01, 0x01, "Flip Screen", IP_KEY_NONE )
	PORT_DIPSETTING(    0x01, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x02, 0x02, "Version", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x00, "Vs" )
	PORT_BITX(    0x04, 0x04, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x04, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x0f, 0x0f, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x05, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x08, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x04, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x01, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0f, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x03, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x07, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0e, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x06, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0x0d, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x0c, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x0b, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0x0a, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x09, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )
	PORT_DIPNAME( 0xf0, 0xf0, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x50, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x80, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x40, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x10, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0xf0, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x30, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x70, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0xe0, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x60, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0xd0, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0xc0, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0xb0, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0xa0, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x90, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Disable" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x04, "Cocktail" )
	PORT_DIPNAME( 0x18, 0x18, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x18, "50000 100000" )
	PORT_DIPSETTING(    0x10, "30000" )
	PORT_DIPSETTING(    0x08, "50000" )
	PORT_DIPSETTING(    0x00, "100000" )
	PORT_DIPNAME( 0x60, 0x60, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x80, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
INPUT_PORTS_END

INPUT_PORTS_START( nemesuk_input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* TEST */
	PORT_DIPNAME( 0x01, 0x01, "Flip Screen", IP_KEY_NONE )
	PORT_DIPSETTING(    0x01, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_DIPNAME( 0x02, 0x02, "Version", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "Normal" )
	PORT_DIPSETTING(    0x00, "Vs" )
	PORT_BITX(    0x04, 0x04, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Service Mode", OSD_KEY_F2, IP_JOY_NONE, 0 )
	PORT_DIPSETTING(    0x04, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x0f, 0x0f, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x05, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x08, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x04, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x01, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0f, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x03, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x07, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0e, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x06, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0x0d, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x0c, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x0b, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0x0a, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x09, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )
	PORT_DIPNAME( 0xf0, 0xf0, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x50, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x80, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x40, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x10, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0xf0, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x30, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x70, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0xe0, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x60, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0xd0, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0xc0, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0xb0, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0xa0, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x90, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Disable" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x02, "Lives", IP_KEY_NONE )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x04, "Cocktail" )
	PORT_DIPNAME( 0x18, 0x18, "Bonus Life", IP_KEY_NONE )
	PORT_DIPSETTING(    0x18, "20000 80000" )
	PORT_DIPSETTING(    0x10, "30000 80000" )
	PORT_DIPSETTING(    0x08, "20000" )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPNAME( 0x60, 0x60, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x80, "Off" )
	PORT_DIPSETTING(    0x00, "On" )
INPUT_PORTS_END

INPUT_PORTS_START( salamand_input_ports )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	2048,	/* 2048 characters */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the two bitplanes are merged in the same nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8     /* every char takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	512,	/* 512 sprites */
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the two bitplanes are merged in the same nibble */
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
			8*4, 9*4, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8     /* every sprite takes 128 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
    { 0, 0x0, &charlayout,   0, 0x80 },	/* the game dynamically modifies this */
    { 0, 0x0, &spritelayout, 0, 0x80 },	/* the game dynamically modifies this */
	{ -1 }
};



static struct AY8910interface ay8910_interface =
{
	2,	/* 2 chip */
	14318000/8,	/* 1.78975 Mhz?? */
	{ 0x20ff, 0x20ff },
	{ nemesis_portA_r, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};



static struct MachineDriver nemesis_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,       /* 8 Mhz?? */
			0,
			readmem,writemem,0,0,
			nemesis_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3072000,       /* 3 Mhz ?? */
			2,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		},
	},

	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	nemesis_vh_start,
	nemesis_vh_stop,
	nemesis_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

static struct MachineDriver salamand_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			8000000,       /* 8 Mhz?? */
			0,
			salamand_readmem,salamand_writemem,0,0,
			salamand_interrupt,1
		},
	},

	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	0,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	nemesis_vh_start,
	nemesis_vh_stop,
	salamand_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_AY8910,
			&ay8910_interface
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( nemesis_rom )
	ROM_REGION(0x40000)    /* 4 * 64k for code and rom */
	ROM_LOAD_EVEN ( "12a_01.bin", 0x00000, 0x008000, 0xa89bc99d )
	ROM_LOAD_ODD  ( "12c_05.bin", 0x00000, 0x008000, 0xe6b869d8 )
	ROM_LOAD_EVEN ( "13a_02.bin", 0x10000, 0x008000, 0x85c767db )
	ROM_LOAD_ODD  ( "13c_06.bin", 0x10000, 0x008000, 0x8fd78319 )
	ROM_LOAD_EVEN ( "14a_03.bin", 0x20000, 0x008000, 0x8e3e63cc )
	ROM_LOAD_ODD  ( "14c_07.bin", 0x20000, 0x008000, 0xb73cbe76 )
	ROM_LOAD_EVEN ( "15a_04.bin", 0x30000, 0x008000, 0x192b4429 )
	ROM_LOAD_ODD  ( "15c_08.bin", 0x30000, 0x008000, 0x78bd2de9 )

	ROM_REGION(0x1000)      /* temporary space for graphics (disposed after conversion) */
	/* empty memory region - not used by the game, but needed because the main */
	/* core currently always frees region #1 after initialization. */

	ROM_REGION(0x10000)    /* 64k for sound */
	ROM_LOAD  ( "09c_snd.bin", 0x0000, 0x4000, 0xc3ea5a3a )
ROM_END

ROM_START( nemesuk_rom )
	ROM_REGION(0x40000)    /* 4 * 64k for code and rom */
	ROM_LOAD_EVEN ( "12a_01.bin", 0x00000, 0x008000, 0x1c18a42a )
	ROM_LOAD_ODD  ( "12c_05.bin", 0x00000, 0x008000, 0x3feaa154 )
	ROM_LOAD_EVEN ( "13a_02.bin", 0x10000, 0x008000, 0x7ba92b17 )
	ROM_LOAD_ODD  ( "13c_06.bin", 0x10000, 0x008000, 0xc07eb1d0 )
	ROM_LOAD_EVEN ( "14a_03.bin", 0x20000, 0x008000, 0x8e3e63cc )
	ROM_LOAD_ODD  ( "14c_07.bin", 0x20000, 0x008000, 0xb73cbe76 )
	ROM_LOAD_EVEN ( "15a_04.bin", 0x30000, 0x008000, 0xc724f74e )
	ROM_LOAD_ODD  ( "15c_08.bin", 0x30000, 0x008000, 0xc4a7a4d9 )

	ROM_REGION(0x1000)      /* temporary space for graphics (disposed after conversion) */
	/* empty memory region - not used by the game, but needed because the main */
	/* core currently always frees region #1 after initialization. */

	ROM_REGION(0x10000)    /* 64k for sound */
	ROM_LOAD  ( "09c_snd.bin", 0x00000, 0x4000, 0xc3ea5a3a )
ROM_END

ROM_START( konamigt_rom )
	ROM_REGION(0x40000)    /* 4 * 64k for code and rom */
	ROM_LOAD_EVEN ( "c01.rom", 0x00000, 0x008000, 0x0563a279 )
	ROM_LOAD_ODD  ( "c05.rom", 0x00000, 0x008000, 0x4253b1a1 )
	ROM_LOAD_EVEN ( "c02.rom", 0x10000, 0x008000, 0xcf5cbd68 )
	ROM_LOAD_ODD  ( "c06.rom", 0x10000, 0x008000, 0x8719ec3b )
	ROM_LOAD_EVEN ( "b03.rom", 0x20000, 0x008000, 0xfe60e84e )
	ROM_LOAD_ODD  ( "b07.rom", 0x20000, 0x008000, 0x22b97847 )
	ROM_LOAD_EVEN ( "b04.rom", 0x30000, 0x008000, 0x33066090 )
	ROM_LOAD_ODD  ( "b08.rom", 0x30000, 0x008000, 0x5263d81d )

	ROM_REGION(0x1000)      /* temporary space for graphics (disposed after conversion) */
	/* empty memory region - not used by the game, but needed because the main */
	/* core currently always frees region #1 after initialization. */

	ROM_REGION(0x10000)    /* 64k for sound */
	ROM_LOAD  ( "b09.rom", 0x00000, 0x4000, 0x02f765f1 )
ROM_END

ROM_START( salamand_rom )
	ROM_REGION(0x200000)    /* 64k for code */
	ROM_LOAD_EVEN ( "18b_d02.bin", 0x00000, 0x010000, 0x880a0c96 )
	ROM_LOAD_ODD  ( "18c_d05.bin", 0x00000, 0x010000, 0x5cfa56e2 )
	ROM_LOAD_EVEN ( "17b_u.bin",   0x20000, 0x010000, 0x0c8444d2 )
	ROM_LOAD_ODD  ( "17c_u.bin",   0x20000, 0x010000, 0x78376965 )

	ROM_REGION(0x1000)      /* temporary space for graphics (disposed after conversion) */
	/* empty memory region - not used by the game, but needed because the main */
	/* core currently always frees region #1 after initialization. */
ROM_END



struct GameDriver nemesis_driver =
{
	__FILE__,
	0,
	"nemesis",
	"Nemesis",
	"????",
	"?????",
	"Allard van der Bas (MAME driver)",
	0,
	&nemesis_machine_driver,

	nemesis_rom,
	0, 0,
	0,
	0,      /* sound_prom */

	nemesis_input_ports,

	0, 0, 0,
	ORIENTATION_DEFAULT,

	0,0
};

struct GameDriver nemesuk_driver =
{
	__FILE__,
	0,
	"nemesuk",
	"Nemesis (UK version)",
	"????",
	"?????",
	"Allard van der Bas (MAME driver)",
	0,
	&nemesis_machine_driver,

	nemesuk_rom,
	0, 0,
	0,
	0,      /* sound_prom */

	nemesuk_input_ports,

	0, 0, 0,
	ORIENTATION_DEFAULT,

	0,0
};

struct GameDriver konamigt_driver =
{
	__FILE__,
	0,
	"konamigt",
	"Konami GT",
	"????",
	"?????",
	"68K test",
	0,
	&nemesis_machine_driver,

	konamigt_rom,
	0, 0,
	0,
	0,      /* sound_prom */

	nemesis_input_ports,

	0, 0, 0,
	ORIENTATION_DEFAULT,

	0,0
};

struct GameDriver salamand_driver =
{
	__FILE__,
	0,
	"salamand",
	"Salamander",
	"????",
	"?????",
	"68K test",
	0,
	&salamand_machine_driver,

	salamand_rom,
	0, 0,
	0,
	0,      /* sound_prom */

	salamand_input_ports,

	0, 0, 0,
	ORIENTATION_DEFAULT,

	0,0
};
