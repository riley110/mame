/*
	Experimental exelvision driver

	Raphael Nabet, 2004

	Exelvision was a French company that designed and sold two computers:
	* EXL 100 (1984)
	* EXELTEL (1986), which is mostly compatible with EXL 100, but has an
	  integrated V23b modem and 5 built-in programs.  Two custom variants of
	  the EXELTEL were designed for chemist's shops and car dealers: they were
	  bundled with application-specific business software, bar code reader,
	  etc.
	These computer were mostly sold in France and in Europe (Spain); there was
	an Arabic version, too.

	Exelvision was founded by former TI employees, which is why their designs
	use TI components and have architectural reminiscences of the primitive
	TI-99/4 design (both computers are built around a microcontroller, have
	little CPU RAM and must therefore store program data in VRAM, and feature
	I/R keyboard and joysticks)

Specs:
	* main CPU is a variant of tms7020 (exl100) or tms7040 (exeltel).  AFAIK,
	  the only difference compared to a stock tms7020/7040 is the SWAP R0
	  instruction is replaced by a custom microcoded LVDP instruction that
	  reads a byte from the VDP VRAM read port; it seems that the first 6 bytes
	  of internal ROM (0xF000-0xF005 on an exeltel) are missing, too.
	* in addition to the internal 128-byte RAM and 2kb (exl100) or 4kb
	  (exeltel) ROM, there are 2kb of CPU RAM and 64(?)kb (exeltel only?) of
	  CPU ROM.
	* I/O is controlled by a tms7041 (exl100) or tms7042 (exeltel) or a variant
	  thereof.  Communication with the main CPU is done through some custom
	  interface (I think), details are still to be worked out.
	* video: tms3556 VDP with 32kb of VRAM (expandable to 64kb), attached to
	  the main CPU.
	* sound: tms5220 speech synthesizer with speech ROM, attached to the I/O
	  CPU
	* keyboard and joystick: an I/R interface controlled by the I/O CPU enables
	  to use a keyboard and two joysticks
	* mass storage: tape interface controlled by the I/O CPU

STATUS:
	* EXL 100 cannot be emulated because the ROMs are not dumped
	* EXELTEL stops early in the boot process and displays a red error screen,
	  presumably because the I/O processor is not emulated

TODO:
	* dump I/O CPU ROM???
	* everything
*/

#include "driver.h"
#include "cpu/tms7000/tms7000.h"
#include "video/tms3556.h"
/*#include "devices/cartslot.h"
#include "devices/cassette.h"*/

static void io_reset(void);

/*
	video initialization
*/
static int video_start_exelv(void)
{
	return tms3556_init(/*0x8000*/0x10000);	/* tms3556 with 32 kb of video RAM */
}

static void machine_reset_exelv(void)
{
	tms3556_reset();
	io_reset();
}

static void exelv_hblank_interrupt(void)
{
	tms3556_interrupt();
}

/*static DEVICE_LOAD(exelv_cart)
{
	return INIT_PASS;
}

static void device_unload_exelv_cart(mess_image *image)
{
}*/

/*
	I/O CPU protocol (WIP):

	I do not have a dump of the I/O CPU ROMs.  The I/O CPU CRC command should
	enable to dump them, but don't take my word for it.

	* port B bit >01 is asserted on reset and after a byte is sent to the I/O
	  CPU.
	* port B bit >02 is asserted after a byte is read from the I/O CPU.  When
	  the I/O  CPU sees this line asserted, it asserts port A bit >01.
	* port A bit >01 is asserted after a byte is sent to CPU (condition
	  cleared when port B bit >01 is cleared after being asserted) and when
	  port B bit >02 is asserted.
	* I/O CPU pulses the main CPU INT1 line when ready to send data; data can
	  be read by the main CPU on the mailbox port (P48).  The data is a
	  function code optionally followed by several bytes of data.  Function
	  codes are:
		>00: unused
		>01: joystick 0 receive
		>02: joystick 1 receive
		>03: speech buffer start
		>04: speech buffer end
		>05: serial
		>06: unused
		>07: introduction screen (logo) (EXL 100 only?) or character
		  definitions
			data byte #1: data length - 1 MSB
			data byte #2: data length - 1 LSB
			data bytes #3 through (data length + 3): graphic data
		>08: I/O cpu initialized
		>09: I/O cpu serial interface ready
		>0a: I/O cpu serial interface not ready
		>0b: screen switched off
		>0c: speech buffer start (EXELTEL only?)
		>0d: speech ROM or I/O cpu CRC check (EXELTEL only?)
			data byte #1: expected CRC MSB
			data byte #2: expected CRC LSB
			data byte #3: data length - 1 MSB
			data byte #4: data length - 1 LSB
			data bytes #5 through (data length + 5): data on which effective
				CRC is computed
		>0e: mailbox test, country code read (EXELTEL only?)
		>0f: speech ROM read (data repeat) (EXELTEL only?)
	* The main CPU sends data to the I/O CPU through the mailbox port (P48).
	  The data byte is a function code; some function codes ask for extra data
	  bytes, which are sent through the mailbox port as well.  Function codes
	  are:
		>00: I/O CPU reset
		>01: NOP (EXELTEL only?)
		>02: read joystick 0 current value
		>03: read joystick 1 current value
		>04: test serial interface availability
		>05: transmit a byte to serial interface
		>06: initialization of serial interface
		>07: read contents of speech ROM (EXELTEL only?)
		>08: reset speech synthesizer
		>09: start speech synthesizer
		>0a: synthesizer data
		>0b: standard generator request
		>0c: I/O CPU CRC (EXELTEL only?)
		>0d: send exelvision logo (EXL 100 only?), start speech ROM sound (EXELTEL only?)
		>0e: data for speech on ROM (EXELTEL only?)
		>0f: do not decode joystick 0 keys (EXELTEL only?)
		>10: do not decode joystick 1 keys (EXELTEL only?)
		>11: decode joystick 0 keys (EXELTEL only?)
		>12: decode joystick 1 keys (EXELTEL only?)
		>13: mailbox test: echo sent data (EXELTEL only?)
		>14: enter sleep mode (EXELTEL only?)
		>15: read country code in speech ROM (EXELTEL only?)
		>16: position I/O CPU DSR without initialization (EXELTEL only?)
		>17: handle speech ROM sound with address (EXELTEL only?)
		other values: I/O CPU reset?
*/

static enum
{
	IOS_NOP,
	IOS_INIT,
	IOS_RESET,
	IOS_STSPEECH1,
	IOS_STSPEECH2,
	IOS_CHARDEF1,
	IOS_CHARDEF2,
	IOS_CHARDEF3,
	IOS_CHARDEF4
} io_state;
static int io_counter;
static int io_command_ack;
static int io_hsk;
static int io_ack;
static int mailbox_out;

static void io_iterate(void);

static void io_reset(void)
{
	/* reset I/O state */
	io_state = IOS_INIT;
	io_command_ack = 1;
	//io_hsk = 0;
	/* clear mailbox */
	mailbox_out = 0x00;
}

static void io_reset_timer(int dummy)
{
	(void) dummy;
	io_reset();
}

static void io_iterate(void)
{
	if (/*io_hsk &&*/ (mailbox_out == 0) && (! io_command_ack))
	{
		switch (io_state)
		{
		case IOS_NOP:
			break;
		case IOS_INIT:
			mailbox_out = 0x08;
			cpunum_set_input_line(0, TMS7000_IRQ1_LINE, PULSE_LINE);
			io_state = IOS_NOP;
			break;
		case IOS_RESET:
			timer_set(ATTOTIME_IN_USEC(100), 0, io_reset_timer);
			break;
		case IOS_STSPEECH1:
			io_state = IOS_STSPEECH2;
			break;
		case IOS_STSPEECH2:
			io_counter++;
			if (io_counter == 24)
				io_state = IOS_NOP;
			break;
		case IOS_CHARDEF1:
			mailbox_out = 0x07;
			cpunum_set_input_line(0, TMS7000_IRQ1_LINE, PULSE_LINE);
			io_state = IOS_CHARDEF2;
			break;
		case IOS_CHARDEF2:
			mailbox_out = 0x04;
			cpunum_set_input_line(0, TMS7000_IRQ1_LINE, PULSE_LINE);
			io_state = IOS_CHARDEF3;
			break;
		case IOS_CHARDEF3:
			mailbox_out = 0xF5;
			cpunum_set_input_line(0, TMS7000_IRQ1_LINE, PULSE_LINE);
			io_state = IOS_CHARDEF4;
			io_counter = 0;
			break;
		case IOS_CHARDEF4:
			{
				static UINT8 fontdata[10*127] =
				{
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x00,0x00,
					0x00,0x50,0x50,0x50,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x50,0x50,0xF8,0x50,0xF8,0x50,0x50,0x00,0x00,
					0x00,0x20,0x70,0xA0,0x70,0x28,0x70,0x20,0x00,0x00,
					0x00,0x48,0xA8,0x50,0x20,0x50,0xA8,0x90,0x00,0x00,
					0x00,0x40,0xA0,0xA0,0x40,0xA8,0x90,0x68,0x00,0x00,
					0x00,0x20,0x20,0x20,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x10,0x20,0x40,0x40,0x40,0x20,0x10,0x00,0x00,
					0x00,0x40,0x20,0x10,0x10,0x10,0x20,0x40,0x00,0x00,
					0x00,0x00,0x88,0x50,0xF8,0x50,0x88,0x00,0x00,0x00,
					0x00,0x00,0x20,0x20,0xF8,0x20,0x20,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x20,0x40,0x00,
					0x00,0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x70,0x20,0x00,
					0x00,0x08,0x08,0x10,0x20,0x40,0x80,0x80,0x00,0x00,
					0x00,0x20,0x50,0x88,0x88,0x88,0x50,0x20,0x00,0x00,
					0x00,0x20,0x60,0xA0,0x20,0x20,0x20,0xF8,0x00,0x00,
					0x00,0x70,0x88,0x08,0x30,0x40,0x80,0xF8,0x00,0x00,
					0x00,0xF8,0x08,0x10,0x30,0x08,0x88,0x70,0x00,0x00,
					0x00,0x10,0x30,0x50,0x90,0xF8,0x10,0x10,0x00,0x00,
					0x00,0xF8,0x80,0xB0,0xC8,0x08,0x88,0x70,0x00,0x00,
					0x00,0x30,0x40,0x80,0xB0,0xC8,0x88,0x70,0x00,0x00,
					0x00,0xF8,0x08,0x10,0x10,0x20,0x40,0x40,0x00,0x00,
					0x00,0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00,0x00,
					0x00,0x70,0x88,0x98,0x68,0x08,0x10,0x60,0x00,0x00,
					0x00,0x00,0x20,0x70,0x20,0x00,0x20,0x70,0x20,0x00,
					0x00,0x00,0x20,0x70,0x20,0x00,0x30,0x20,0x40,0x00,
					0x00,0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x00,0x00,
					0x00,0x00,0x00,0xF8,0x00,0xF8,0x00,0x00,0x00,0x00,
					0x00,0x40,0x20,0x10,0x08,0x10,0x20,0x40,0x00,0x00,
					0x00,0x70,0x88,0x10,0x20,0x20,0x00,0x20,0x00,0x00,
					0x00,0x70,0x88,0x98,0xA8,0xB0,0x80,0x70,0x00,0x00,
					//0x00,0x20,0x50,0x88,0x88,0xF8,0x88,0x88,0x00,0x00,// original MAME font
					//0x00,0x1C,0x22,0x22,0x3E,0x22,0x22,0x22,0x00,0x00,// original exeltel font
					0x00,0x70,0x88,0x88,0xF8,0x88,0x88,0x88,0x00,0x00,	// original exeltel font shifted left twice
					0x00,0xF0,0x48,0x48,0x70,0x48,0x48,0xF0,0x00,0x00,
					0x00,0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00,0x00,
					0x00,0xF0,0x48,0x48,0x48,0x48,0x48,0xF0,0x00,0x00,
					0x00,0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8,0x00,0x00,
					0x00,0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0x00,0x00,
					0x00,0x70,0x88,0x80,0x80,0x98,0x88,0x70,0x00,0x00,
					0x00,0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x00,0x00,
					0x00,0x70,0x20,0x20,0x20,0x20,0x20,0x70,0x00,0x00,
					0x00,0x38,0x10,0x10,0x10,0x10,0x90,0x60,0x00,0x00,
					0x00,0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88,0x00,0x00,
					0x00,0x80,0x80,0x80,0x80,0x80,0x80,0xF8,0x00,0x00,
					0x00,0x88,0x88,0xD8,0xA8,0x88,0x88,0x88,0x00,0x00,
					0x00,0x88,0x88,0xC8,0xA8,0x98,0x88,0x88,0x00,0x00,
					0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x00,
					0x00,0xF0,0x88,0x88,0xF0,0x80,0x80,0x80,0x00,0x00,
					0x00,0x70,0x88,0x88,0x88,0x88,0xA8,0x70,0x08,0x00,
					0x00,0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88,0x00,0x00,
					0x00,0x70,0x88,0x80,0x70,0x08,0x88,0x70,0x00,0x00,
					0x00,0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,
					0x00,0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x00,
					0x00,0x88,0x88,0x88,0x50,0x50,0x50,0x20,0x00,0x00,
					0x00,0x88,0x88,0x88,0xA8,0xA8,0xD8,0x88,0x00,0x00,
					0x00,0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00,0x00,
					0x00,0x88,0x88,0x50,0x20,0x20,0x20,0x20,0x00,0x00,
					0x00,0xF8,0x08,0x10,0x20,0x40,0x80,0xF8,0x00,0x00,
					0x00,0x70,0x40,0x40,0x40,0x40,0x40,0x70,0x00,0x00,
					0x00,0x80,0x80,0x40,0x20,0x10,0x08,0x08,0x00,0x00,
					0x00,0x70,0x10,0x10,0x10,0x10,0x10,0x70,0x00,0x00,
					0x00,0x20,0x50,0x88,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x00,
					0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x70,0x08,0x78,0x88,0x78,0x00,0x00,
					0x00,0x80,0x80,0xB0,0xC8,0x88,0xC8,0xB0,0x00,0x00,
					0x00,0x00,0x00,0x70,0x88,0x80,0x88,0x70,0x00,0x00,
					0x00,0x08,0x08,0x68,0x98,0x88,0x98,0x68,0x00,0x00,
					0x00,0x00,0x00,0x70,0x88,0xF8,0x80,0x70,0x00,0x00,
					0x00,0x30,0x48,0x40,0xF0,0x40,0x40,0x40,0x00,0x00,
					0x00,0x00,0x00,0x78,0x88,0x88,0x78,0x08,0x88,0x70,
					0x00,0x80,0x80,0xB0,0xC8,0x88,0x88,0x88,0x00,0x00,
					0x00,0x20,0x00,0x60,0x20,0x20,0x20,0x70,0x00,0x00,
					0x00,0x08,0x00,0x18,0x08,0x08,0x08,0x48,0x48,0x30,
					0x00,0x80,0x80,0x88,0x90,0xE0,0x90,0x88,0x00,0x00,
					0x00,0x60,0x20,0x20,0x20,0x20,0x20,0x70,0x00,0x00,
					0x00,0x00,0x00,0xD0,0xA8,0xA8,0xA8,0x88,0x00,0x00,
					0x00,0x00,0x00,0xB0,0xC8,0x88,0x88,0x88,0x00,0x00,
					0x00,0x00,0x00,0x70,0x88,0x88,0x88,0x70,0x00,0x00,
					0x00,0x00,0x00,0xB0,0xC8,0x88,0xC8,0xB0,0x80,0x80,
					0x00,0x00,0x00,0x68,0x98,0x88,0x98,0x68,0x08,0x08,
					0x00,0x00,0x00,0xB0,0xC8,0x80,0x80,0x80,0x00,0x00,
					0x00,0x00,0x00,0x70,0x80,0x70,0x08,0xF0,0x00,0x00,
					0x00,0x40,0x40,0xF0,0x40,0x40,0x48,0x30,0x00,0x00,
					0x00,0x00,0x00,0x88,0x88,0x88,0x98,0x68,0x00,0x00,
					0x00,0x00,0x00,0x88,0x88,0x50,0x50,0x20,0x00,0x00,
					0x00,0x00,0x00,0x88,0x88,0xA8,0xA8,0x50,0x00,0x00,
					0x00,0x00,0x00,0x88,0x50,0x20,0x50,0x88,0x00,0x00,
					0x00,0x00,0x00,0x88,0x88,0x98,0x68,0x08,0x88,0x70,
					0x00,0x00,0x00,0xF8,0x10,0x20,0x40,0xF8,0x00,0x00,
					0x00,0x18,0x20,0x10,0x60,0x10,0x20,0x18,0x00,0x00,
					0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,
					0x00,0x60,0x10,0x20,0x18,0x20,0x10,0x60,0x00,0x00,
					0x00,0x48,0xA8,0x90,0x00,0x00,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				};
				mailbox_out = fontdata[io_counter+9-2*(io_counter%10)] >> 2;
				cpunum_set_input_line(0, TMS7000_IRQ1_LINE, PULSE_LINE);
				io_counter++;
				if (io_counter == 127*10)
					io_state = IOS_NOP;
			}
			break;
		}
	}
}

static void set_io_hsk(int state)
{
	if (state != io_hsk)
	{
		io_hsk = state;
		if (io_command_ack)
		{
			if (io_hsk)
				cpunum_set_input_line(0, TMS7000_IRQ1_LINE, PULSE_LINE);
			else
			{
				io_command_ack = 0;
				io_iterate();
			}
		}
	}
}

static void set_io_ack(int state)
{
	if (state != io_ack)
	{
		io_ack = state;
		/* ??? */
	}
}

static READ8_HANDLER(mailbox_r)
{
	int reply = mailbox_out;

	/* clear mailbox */
	mailbox_out = 0x00;

	/* see if there are other messages to post */
	io_iterate();

	return reply;
}

static WRITE8_HANDLER(mailbox_w)
{
	switch (io_state)
	{
	case IOS_STSPEECH2:
		/*...*/
		io_command_ack = 1;
		break;
	default:
		switch (data)
		{
		case 0x09:
			io_state = IOS_STSPEECH1;
			io_counter = 0;
			io_command_ack = 1;
			break;
		case 0x0b:
			io_state = IOS_CHARDEF1;
			io_command_ack = 1;
			break;
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x0a:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			logerror("unemulated I/O command %d", (int) data);
		case 0x00:
		default:
			io_state = IOS_RESET;
			io_command_ack = 1;
			break;
		}
	}
}

static READ8_HANDLER(exelv_porta_r)
{
	return (io_command_ack || io_ack) ? 1 : 0;
}

static WRITE8_HANDLER(exelv_portb_w)
{
	set_io_hsk((data & 0x1) != 0);
	set_io_ack((data & 0x2) != 0);
}

/*
	Main CPU memory map summary:

	@>0000-@>007f: tms7020/tms7040 internal RAM
	@>0080-@>00ff: reserved
	@>0100-@>010b: tms7020/tms7040 internal I/O ports
		@>104 (P4): port A
		@>106 (P6): port B
			bit >04: page select bit 0 (LSBit)
	@>010c-@>01ff: external I/O ports?
		@>012d (P45): tms3556 control write port???
		@>012e (P46): tms3556 VRAM write port???
		@>0130 (P48): I/O CPU communication port R/W ("mailbox")
		@>0138 (P56): read sets page select bit 1, write clears it???
		@>0139 (P57): read sets page select bit 2 (MSBit), write clears it???
		@>0140 (P64)
			bit >40: enable page select bit 1 and 2 (MSBits)
	@>0200-@>7fff: system ROM? (two pages?) + cartridge ROMs? (one or two pages?)
	@>8000-@>bfff: free for expansion?
	@>c000-@>c7ff: CPU RAM?
	@>c800-@>efff: free for expansion?
	@>f000-@>f7ff: tms7040 internal ROM
	@>f800-@>ffff: tms7020/tms7040 internal ROM
*/

static ADDRESS_MAP_START(exelv_memmap, ADDRESS_SPACE_PROGRAM, 8)

	AM_RANGE(0x0000, 0x007f) AM_READWRITE(tms7000_internal_r, tms7000_internal_w)/* tms7020 internal RAM */
	AM_RANGE(0x0080, 0x00ff) AM_READWRITE(MRA8_NOP, MWA8_NOP)		/* reserved */
	AM_RANGE(0x0100, 0x010b) AM_READWRITE(tms70x0_pf_r, tms70x0_pf_w)/* tms7020 internal I/O ports */
	//AM_RANGE(0x010c, 0x01ff) AM_READWRITE(MRA8_NOP, MWA8_NOP)		/* external I/O ports */
	AM_RANGE(0x012d, 0x0012d) AM_READWRITE(tms3556_reg_r/*right???*/, tms3556_reg_w)
	AM_RANGE(0x012e, 0x0012e) AM_READWRITE(tms3556_vram_r/*right???*/, tms3556_vram_w)
	AM_RANGE(0x0130, 0x00130) AM_READWRITE(mailbox_r, mailbox_w)
	AM_RANGE(0x0200, 0x7fff) AM_READWRITE(MRA8_ROM, MWA8_ROM)		/* system ROM */
	AM_RANGE(0x8000, 0xbfff) AM_READWRITE(MRA8_NOP, MWA8_NOP)
	AM_RANGE(0xc000, 0xc7ff) AM_READWRITE(MRA8_RAM, MWA8_RAM)		/* CPU RAM */
	AM_RANGE(0xc800, /*0xf7ff*/0xefff) AM_READWRITE(MRA8_NOP, MWA8_NOP)
	AM_RANGE(/*0xf800*/0xf000, 0xffff) AM_READWRITE(MRA8_ROM, MWA8_ROM)/* tms7020 internal ROM */

ADDRESS_MAP_END

static ADDRESS_MAP_START(exelv_portmap, ADDRESS_SPACE_IO, 8)

	AM_RANGE(TMS7000_PORTA, TMS7000_PORTA) AM_READ(exelv_porta_r)
	AM_RANGE(TMS7000_PORTB, TMS7000_PORTB) AM_WRITE(exelv_portb_w)

ADDRESS_MAP_END


/* keyboard: ??? */
static INPUT_PORTS_START(exelv)

	PORT_START

INPUT_PORTS_END


static MACHINE_DRIVER_START(exelv)

	/* basic machine hardware */
	/* TMS7020 CPU @ 4.91(?) MHz */
	MDRV_CPU_ADD(TMS7000_EXL, 4910000)
	/*MDRV_CPU_CONFIG(0)*/
	MDRV_CPU_PROGRAM_MAP(exelv_memmap, 0)
	MDRV_CPU_IO_MAP(exelv_portmap, 0)
	MDRV_CPU_VBLANK_INT(exelv_hblank_interrupt, 363)
	/*MDRV_CPU_PERIODIC_INT(func, rate)*/

	MDRV_SCREEN_REFRESH_RATE(50)
	MDRV_SCREEN_VBLANK_TIME(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	/*MDRV_INTERLEAVE(interleave)*/

	MDRV_MACHINE_RESET( exelv )
	/*MDRV_NVRAM_HANDLER( NULL )*/

	/* video hardware */
	MDRV_VIDEO_START(exelv)
	MDRV_TMS3556

	/* sound */
	/*MDRV_SOUND_ADD(TMS5220, tms5220interface)*/

MACHINE_DRIVER_END


/*
  ROM loading
*/
ROM_START(exeltel)
	/*CPU memory space*/
	ROM_REGION(0x10000,REGION_CPU1,0)
//	ROM_LOAD("exeltel14.bin", 0x0000, 0x8000, CRC(52a80dd4))      /* system ROM */
	ROM_LOAD("guppy.bin", 0x6000, 0x2000, CRC(c3a3e6d9))          /* cartridge (test) */
	ROM_LOAD("exeltelin.bin", 0xf006, 0x0ffa, CRC(c12f24b5))      /* internal ROM */
ROM_END

SYSTEM_CONFIG_START(exelv)

	/* cartridge port is not emulated */

SYSTEM_CONFIG_END

/*		YEAR	NAME	PARENT		COMPAT	MACHINE		INPUT	INIT	CONFIG		COMPANY			FULLNAME */
/*COMP(	1984,	exl100,	0,			0,		exelv,		exelv,	NULL,	exelv,		"Exelvision",	"exl 100" , 0)*/
COMP(	1986,	exeltel,0/*exl100*/,0,		exelv,		exelv,	NULL,	exelv,		"Exelvision",	"exeltel" , 0)
