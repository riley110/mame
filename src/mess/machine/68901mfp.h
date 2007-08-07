#ifndef MFP68901_H
#define MFP68901_H

#define MAX_MFP	4

#include "driver.h"
#include "timer.h"

struct mfp68901_interface
{
	int	chip_clock;
	int	timer_clock;

	void (*tao_w)(int which, int value);
	void (*tbo_w)(int which, int value);
	void (*tco_w)(int which, int value);
	void (*tdo_w)(int which, int value);

	void (*irq_callback)(int which, int state, int vector);

	read8_handler gpio_r;
	write8_handler gpio_w;
};

enum
{
	MFP68901_INT_GPI0 = 0,
	MFP68901_INT_GPI1,
	MFP68901_INT_GPI2,
	MFP68901_INT_GPI3,
	MFP68901_INT_TIMER_D,
	MFP68901_INT_TIMER_C,
	MFP68901_INT_GPI4,
	MFP68901_INT_GPI5,
	MFP68901_INT_TIMER_B,
	MFP68901_INT_XMIT_ERROR,
	MFP68901_INT_XMIT_BUFFER_EMPTY,
	MFP68901_INT_RCV_ERROR,
	MFP68901_INT_RCV_BUFFER_FULL,
	MFP68901_INT_TIMER_A,
	MFP68901_INT_GPI6,
	MFP68901_INT_GPI7
};


#define MFP68901_AER_GPIP_0				0x01
#define MFP68901_AER_GPIP_1				0x02
#define MFP68901_AER_GPIP_2				0x04
#define MFP68901_AER_GPIP_3				0x08
#define MFP68901_AER_GPIP_4				0x10
#define MFP68901_AER_GPIP_5				0x20
#define MFP68901_AER_GPIP_6				0x40
#define MFP68901_AER_GPIP_7				0x80

#define MFP68901_VR_S					0x08

#define MFP68901_IPRB_GPIP_0			0x01
#define MFP68901_IPRB_GPIP_1			0x02
#define MFP68901_IPRB_GPIP_2			0x04
#define MFP68901_IPRB_GPIP_3			0x08
#define MFP68901_IPRB_TIMER_D			0x10
#define MFP68901_IPRB_TIMER_C			0x20
#define MFP68901_IPRB_GPIP_4			0x40
#define MFP68901_IPRB_GPIP_5			0x80

#define MFP68901_IPRA_TIMER_B			0x01
#define MFP68901_IPRA_XMIT_ERROR		0x02
#define MFP68901_IPRA_XMIT_BUFFER_EMPTY	0x04
#define MFP68901_IPRA_RCV_ERROR			0x08
#define MFP68901_IPRA_RCV_BUFFER_FULL	0x10
#define MFP68901_IPRA_TIMER_A			0x20
#define MFP68901_IPRA_GPIP_6			0x40
#define MFP68901_IPRA_GPIP_7			0x80

#define MFP68901_TCR_TIMER_STOPPED		0x00
#define MFP68901_TCR_TIMER_DELAY_4		0x01
#define MFP68901_TCR_TIMER_DELAY_10		0x02
#define MFP68901_TCR_TIMER_DELAY_16		0x03
#define MFP68901_TCR_TIMER_DELAY_50		0x04
#define MFP68901_TCR_TIMER_DELAY_64		0x05
#define MFP68901_TCR_TIMER_DELAY_100	0x06
#define MFP68901_TCR_TIMER_DELAY_200	0x07
#define MFP68901_TCR_TIMER_EVENT		0x08
#define MFP68901_TCR_TIMER_PULSE_4		0x09
#define MFP68901_TCR_TIMER_PULSE_10		0x0a
#define MFP68901_TCR_TIMER_PULSE_16		0x0b
#define MFP68901_TCR_TIMER_PULSE_50		0x0c
#define MFP68901_TCR_TIMER_PULSE_64		0x0d
#define MFP68901_TCR_TIMER_PULSE_100	0x0e
#define MFP68901_TCR_TIMER_PULSE_200	0x0f
#define MFP68901_TCR_TIMER_RESET		0x10

#define MFP68901_UCR_PARITY_ENABLED		0x04
#define MFP68901_UCR_PARITY_EVEN		0x02
#define MFP68901_UCR_PARITY_ODD			0x00
#define MFP68901_UCR_WORD_LENGTH_8		0x00
#define MFP68901_UCR_WORD_LENGTH_7		0x20
#define MFP68901_UCR_WORD_LENGTH_6		0x40
#define MFP68901_UCR_WORD_LENGTH_5		0x60
#define MFP68901_UCR_START_STOP_0_0		0x00
#define MFP68901_UCR_START_STOP_1_1		0x08
#define MFP68901_UCR_START_STOP_1_15	0x10
#define MFP68901_UCR_START_STOP_1_2		0x18
#define MFP68901_UCR_CLOCK_DIVIDE_16	0x80
#define MFP68901_UCR_CLOCK_DIVIDE_1		0x00

#define MFP68901_RSR_RCV_ENABLE			0x01
#define MFP68901_RSR_SYNC_STRIP_ENABLE	0x02
#define MFP68901_RSR_MCIP				0x04
#define MFP68901_RSR_FOUND_SEARCH		0x08
#define MFP68901_RSR_FRAME_ERROR		0x10
#define MFP68901_RSR_PARITY_ERROR		0x20
#define MFP68901_RSR_OVERRUN_ERROR		0x40
#define MFP68901_RSR_BUFFER_FULL		0x80

#define MFP68901_TSR_XMIT_ENABLE		0x01
#define MFP68901_TSR_LOW				0x02
#define MFP68901_TSR_HIGH				0x04
#define MFP68901_TSR_BREAK				0x08
#define MFP68901_TSR_END_OF_XMIT		0x10
#define MFP68901_TSR_AUTO_TURNAROUND	0x20
#define MFP68901_TSR_UNDERRUN_ERROR		0x40
#define MFP68901_TSR_BUFFER_EMPTY		0x80

void mfp68901_config(int which, const struct mfp68901_interface *intf);

void mfp68901_tai_w(int which, int value);
void mfp68901_tbi_w(int which, int value);

READ16_HANDLER( mfp68901_0_register16_r );
READ16_HANDLER( mfp68901_1_register16_r );
READ16_HANDLER( mfp68901_2_register16_r );
READ16_HANDLER( mfp68901_3_register16_r );

WRITE16_HANDLER( mfp68901_0_register_msb_w );
WRITE16_HANDLER( mfp68901_1_register_msb_w );
WRITE16_HANDLER( mfp68901_2_register_msb_w );
WRITE16_HANDLER( mfp68901_3_register_msb_w );

WRITE16_HANDLER( mfp68901_0_register_lsb_w );
WRITE16_HANDLER( mfp68901_1_register_lsb_w );
WRITE16_HANDLER( mfp68901_2_register_lsb_w );
WRITE16_HANDLER( mfp68901_3_register_lsb_w );

#endif
