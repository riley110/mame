/* 
    machine/rmnimbus.c
    
    Machine driver for the Research Machines Nimbus.
    
    Phill Harvey-Smith
    2009-11-29.
    
    80186 internal DMA/Timer/PIC code borrowed from Compis driver.
    Perhaps this needs merging into the 80186 core.....
*/


#include "driver.h"
#include "memory.h"
#include "cpu/i86/i86.h"
#include "debug/debugcpu.h"
#include "debug/debugcon.h"
#include "devices/flopdrv.h"
#include "machine/pic8259.h"
#include "machine/pit8253.h"
#include "machine/msm8251.h"
#include "machine/z80sio.h"

#include "includes/rmnimbus.h"

/*-------------------------------------------------------------------------*/
/* Defines, constants, and global variables                                */
/*-------------------------------------------------------------------------*/

/* CPU 80186 */
#define LATCH_INTS 1
#define LOG_PORTS 0
#define LOG_INTERRUPTS 0
#define LOG_INTERRUPTS_EXT 0
#define LOG_TIMER 0
#define LOG_OPTIMIZATION 0
#define LOG_DMA 1
#define CPU_RESUME_TRIGGER	7123

/* 80186 internal stuff */
struct mem_state
{
	UINT16	    lower;
	UINT16	    upper;
	UINT16	    middle;
	UINT16	    middle_size;
	UINT16	    peripheral;
};

struct timer_state
{
	UINT16	    control;
	UINT16	    maxA;
	UINT16	    maxB;
	UINT16	    count;
	emu_timer   *int_timer;
	emu_timer   *time_timer;
	UINT8	    time_timer_active;
	attotime	last_time;
};

struct dma_state
{
	UINT32	    source;
	UINT32	    dest;
	UINT16	    count;
	UINT16	    control;
	UINT8	    finished;
	emu_timer   *finish_timer;
};

struct intr_state
{
	UINT8	pending;
	UINT16	ack_mask;
	UINT16	priority_mask;
	UINT16	in_service;
	UINT16	request;
	UINT16	status;
	UINT16	poll_status;
	UINT16	timer;
	UINT16	dma[2];
	UINT16	ext[4];
    UINT16  ext_vector[2]; // external vectors, when in cascade mode
};

static struct i186_state
{
	struct timer_state	timer[3];
	struct dma_state	dma[2];
	struct intr_state	intr;
	struct mem_state	mem;
} i186;


/* Z80 SIO */

const z80sio_interface sio_intf =
{
	sio_interrupt,			/* interrupt handler */
	0, //sio_dtr_w,				/* DTR changed handler */
	0,						/* RTS changed handler */
	0,						/* BREAK changed handler */
	sio_serial_transmit,	/* transmit handler */
	sio_serial_receive		/* receive handler */
};

static UINT8    keyrows[NIMBUS_KEYROWS];

/* Floppy drives WD2793 */

static WRITE_LINE_DEVICE_HANDLER( nimbus_fdc_intrq_w );
static WRITE_LINE_DEVICE_HANDLER( nimbus_fdc_drq_w );

const wd17xx_interface nimbus_wd17xx_interface =
{
	DEVCB_LINE(nimbus_fdc_intrq_w),
	DEVCB_LINE(nimbus_fdc_drq_w),
	{FLOPPY_0, FLOPPY_1, FLOPPY_2, FLOPPY_3}
};

static struct _nimbus_fdc
{
    UINT8 drq_enabled;
    UINT8 irq_enabled;
    UINT8 diskno;
    UINT8 motor_on;
} nimbus_fdc;


static void drq_callback(running_machine *machine, int which);

static void execute_debug_irq(running_machine *machine, int ref, int params, const char *param[]);
static void execute_debug_intmasks(running_machine *machine, int ref, int params, const char *param[]);
static void execute_debug_key(running_machine *machine, int ref, int params, const char *param[]);

static int instruction_hook(const device_config *device, offs_t curpc);
static void decode_subbios(const device_config *device,offs_t pc);
static void decode_dssi_f_fill_area(const device_config *device,UINT16  ds, UINT16 si);
static void decode_dssi_f_plot_character_string(const device_config *device,UINT16  ds, UINT16 si);
static void decode_dssi_f_set_new_clt(const device_config *device,UINT16  ds, UINT16 si);
static void fdc_reset(void);


#define num_ioports 0x80
static UINT16   IOPorts[num_ioports];

UINT8 nextkey;
UINT8 sio_int_state;

/*************************************
 *
 *  80186 interrupt controller
 *
 *************************************/
static IRQ_CALLBACK(int_callback)
{
    UINT8   vector;
    UINT16  old;
    UINT16  oldreq;

	if (LOG_INTERRUPTS)
		logerror("(%f) **** Acknowledged interrupt vector %02X\n", attotime_to_double(timer_get_time(device->machine)), i186.intr.poll_status & 0x1f);

	/* clear the interrupt */
	cpu_set_input_line(device, 0, CLEAR_LINE);
	i186.intr.pending = 0;

    oldreq=i186.intr.request;

	/* clear the request and set the in-service bit */
#if LATCH_INTS
	i186.intr.request &= ~i186.intr.ack_mask;
#else
	i186.intr.request &= ~(i186.intr.ack_mask & 0x0f);
#endif

    if(i186.intr.request!=oldreq)
        logerror("i186.intr.request changed from %02X to %02X\n",oldreq,i186.intr.request);

    old=i186.intr.in_service;

	i186.intr.in_service |= i186.intr.ack_mask;
	
    if (LOG_INTERRUPTS)
        if(i186.intr.in_service!=old)
            logerror("i186.intr.in_service changed from %02X to %02X\n",old,i186.intr.in_service);
    
    if (i186.intr.ack_mask == 0x0001)
	{
		switch (i186.intr.poll_status & 0x1f)
		{
			case 0x08:	i186.intr.status &= ~0x01;	break;
			case 0x12:	i186.intr.status &= ~0x02;	break;
			case 0x13:	i186.intr.status &= ~0x04;	break;
		}
	}
	i186.intr.ack_mask = 0;

	/* a request no longer pending */
	i186.intr.poll_status &= ~0x8000;

	/* return the vector */
    switch(i186.intr.poll_status & 0x1F)
    {
        case 0x0C   : vector=(i186.intr.ext[0] & EXTINT_CTRL_CASCADE) ? i186.intr.ext_vector[0] : (i186.intr.poll_status & 0x1f); logerror("int 0x0c vector=%02X\n",vector); break;
        case 0x0D   : vector=(i186.intr.ext[1] & EXTINT_CTRL_CASCADE) ? i186.intr.ext_vector[1] : (i186.intr.poll_status & 0x1f); logerror("int 0x0D vector=%02X\n",vector); break;
        default :
            vector=i186.intr.poll_status & 0x1f; break;
    }

    if (LOG_INTERRUPTS_EXT)
	{
        logerror("i186.intr.ext[0]=%04X i186.intr.ext[1]=%04X\n",i186.intr.ext[0],i186.intr.ext[1]);
        logerror("Ext vectors : %02X %02X\n",i186.intr.ext_vector[0],i186.intr.ext_vector[1]);
        logerror("Calling vector %02X\n",vector);
    }
    
    return vector;
}


static void update_interrupt_state(running_machine *machine)
{   
    int new_vector = 0;
    int Priority;
    int IntNo;

	if (LOG_INTERRUPTS) 
        logerror("update_interrupt_status: req=%04X stat=%04X serv=%04X priority_mask=%4X\n", i186.intr.request, i186.intr.status, i186.intr.in_service, i186.intr.priority_mask);

	/* loop over priorities */
	for (Priority = 0; Priority <= i186.intr.priority_mask; Priority++)
	{
		/* note: by checking 4 bits, we also verify that the mask is off */
		if ((i186.intr.timer & 15) == Priority)
		{
			/* if we're already servicing something at this level, don't generate anything new */
			if (i186.intr.in_service & 0x01)
				return;

			/* if there's something pending, generate an interrupt */
			if (i186.intr.status & 0x07)
			{
				if (i186.intr.status & 1)
					new_vector = 0x08;
				else if (i186.intr.status & 2)
					new_vector = 0x12;
				else if (i186.intr.status & 4)
					new_vector = 0x13;
				else
					popmessage("Invalid timer interrupt!");

				/* set the clear mask and generate the int */
				i186.intr.ack_mask = 0x0001;
				goto generate_int;
			}
		}

		/* check DMA interrupts */
		for (IntNo = 0; IntNo < 2; IntNo++)
			if ((i186.intr.dma[IntNo] & 0x0F) == Priority)
			{
				/* if we're already servicing something at this level, don't generate anything new */
				if (i186.intr.in_service & (0x04 << IntNo))
					return;

				/* if there's something pending, generate an interrupt */
				if (i186.intr.request & (0x04 << IntNo))
				{
					new_vector = 0x0a + IntNo;

					/* set the clear mask and generate the int */
					i186.intr.ack_mask = 0x0004 << IntNo;
					goto generate_int;
				}
			}

        //logerror("Checking external ints\n");
		/* check external interrupts */
		for (IntNo = 0; IntNo < 4; IntNo++)
			if ((i186.intr.ext[IntNo] & 0x0F) == Priority)
			{
                if (LOG_INTERRUPTS)
                    logerror("Int%d priority=%d\n",IntNo,Priority);
                
                /* if we're already servicing something at this level, don't generate anything new */
				if (i186.intr.in_service & (0x10 << IntNo))
					return;

				/* if there's something pending, generate an interrupt */
				if (i186.intr.request & (0x10 << IntNo))
				{
					/* otherwise, generate an interrupt for this request */
					new_vector = 0x0c + IntNo;

					/* set the clear mask and generate the int */
					i186.intr.ack_mask = 0x0010 << IntNo;
                    logerror("External int !\n");
					goto generate_int;
				}
			}
	}
	return;

generate_int:
	/* generate the appropriate interrupt */
	i186.intr.poll_status = 0x8000 | new_vector;
	if (!i186.intr.pending)
		cputag_set_input_line(machine, MAINCPU_TAG, 0, ASSERT_LINE);
	i186.intr.pending = 1;
	cpuexec_trigger(machine, CPU_RESUME_TRIGGER);
	if (LOG_OPTIMIZATION) logerror("  - trigger due to interrupt pending\n");
	if (LOG_INTERRUPTS) logerror("(%f) **** Requesting interrupt vector %02X\n", attotime_to_double(timer_get_time(machine)), new_vector);
}


static void handle_eoi(running_machine *machine,int data)
{
    int Priority;
    int IntNo;

	/* specific case */
	if (!(data & 0x8000))
	{
		/* turn off the appropriate in-service bit */
		switch (data & 0x1f)
		{
			case 0x08:	i186.intr.in_service &= ~0x01;	break;
			case 0x12:	i186.intr.in_service &= ~0x01;	break;
			case 0x13:	i186.intr.in_service &= ~0x01;	break;
			case 0x0a:	i186.intr.in_service &= ~0x04;	break;
			case 0x0b:	i186.intr.in_service &= ~0x08;	break;
			case 0x0c:	i186.intr.in_service &= ~0x10;	break;
			case 0x0d:	i186.intr.in_service &= ~0x20;	break;
			case 0x0e:	i186.intr.in_service &= ~0x40;	break;
			case 0x0f:	i186.intr.in_service &= ~0x80;	break;
			default:	logerror("%05X:ERROR - 80186 EOI with unknown vector %02X\n", cpu_get_pc(cputag_get_cpu(machine, MAINCPU_TAG)), data & 0x1f);
		}
		if (LOG_INTERRUPTS) logerror("(%f) **** Got EOI for vector %02X\n", attotime_to_double(timer_get_time(machine)), data & 0x1f);
	}

	/* non-specific case */
	else
	{
		/* loop over priorities */
		for (Priority = 0; Priority <= 7; Priority++)
		{
			/* check for in-service timers */
			if ((i186.intr.timer & 0x07) == Priority && (i186.intr.in_service & 0x01))
			{
				i186.intr.in_service &= ~0x01;
				if (LOG_INTERRUPTS) logerror("(%f) **** Got EOI for timer\n", attotime_to_double(timer_get_time(machine)));
				return;
			}

			/* check for in-service DMA interrupts */
			for (IntNo = 0; IntNo < 2; IntNo++)
				if ((i186.intr.dma[IntNo] & 0x07) == Priority && (i186.intr.in_service & (0x04 << IntNo)))
				{
					i186.intr.in_service &= ~(0x04 << IntNo);
					if (LOG_INTERRUPTS) logerror("(%f) **** Got EOI for DMA%d\n", attotime_to_double(timer_get_time(machine)), IntNo);
					return;
				}

			/* check external interrupts */
			for (IntNo = 0; IntNo < 4; IntNo++)
				if ((i186.intr.ext[IntNo] & 0x07) == Priority && (i186.intr.in_service & (0x10 << IntNo)))
				{
					i186.intr.in_service &= ~(0x10 << IntNo);
					if (LOG_INTERRUPTS) logerror("(%f) **** Got EOI for INT%d\n", attotime_to_double(timer_get_time(machine)), IntNo);
					return;
				}
		}
	}
}

/* Trigger an external interupt, optionally supplying the vector to take */
static void external_int(running_machine *machine, UINT16 intno, UINT8 vector)
{
	if (LOG_INTERRUPTS_EXT) logerror("generating external int %02X, vector %02X\n",intno,vector);
 
   // Only 4 external ints
    if(intno>3)
    {
        logerror("external_int() invalid external interupt no : 0x%02X (can only be 0..3)\n",intno);
        return;
    }
    
    // Only set external vector if cascade mode enabled, only valid for
    // int 0 & int 1
    if (intno<2)
    {
        if(i186.intr.ext[intno] & EXTINT_CTRL_CASCADE)
            i186.intr.ext_vector[intno]=vector;
    }
    
    // Turn on the requested request bit and handle interrupt
    i186.intr.request |= (0x010 << intno);
    update_interrupt_state(machine);
}


/*************************************
 *
 *  80186 internal timers
 *
 *************************************/

static TIMER_CALLBACK(internal_timer_int)
{
	int which = param;
	struct timer_state *t = &i186.timer[which];

	if (LOG_TIMER) logerror("Hit interrupt callback for timer %d\n", which);

	/* set the max count bit */
	t->control |= 0x0020;

	/* request an interrupt */
	if (t->control & 0x2000)
	{
		i186.intr.status |= 0x01 << which;
		update_interrupt_state(machine);
		if (LOG_TIMER) logerror("  Generating timer interrupt\n");
	}

	/* if we're continuous, reset */
	if (t->control & 0x0001)
	{
		int count = t->maxA ? t->maxA : 0x10000;
		timer_adjust_oneshot(t->int_timer, attotime_mul(ATTOTIME_IN_HZ(2000000), count), which);
		if (LOG_TIMER) logerror("  Repriming interrupt\n");
	}
	else
		timer_adjust_oneshot(t->int_timer, attotime_never, which);
}


static void internal_timer_sync(int which)
{
	struct timer_state *t = &i186.timer[which];

	/* if we have a timing timer running, adjust the count */
	if (t->time_timer_active)
	{
		attotime current_time = timer_timeelapsed(t->time_timer);
		int net_clocks = attotime_mul(attotime_sub(current_time, t->last_time),  2000000).seconds;
		t->last_time = current_time;

		/* set the max count bit if we passed the max */
		if ((int)t->count + net_clocks >= t->maxA)
			t->control |= 0x0020;

		/* set the new count */
		if (t->maxA != 0)
			t->count = (t->count + net_clocks) % t->maxA;
		else
			t->count = t->count + net_clocks;
	}
}


static void internal_timer_update(running_machine *machine,
								  int which,
                                  int new_count,
                                  int new_maxA,
                                  int new_maxB,
                                  int new_control)
{
	struct timer_state *t = &i186.timer[which];
	int update_int_timer = 0;

    logerror("internal_timer_update: %d, new_count=%d, new_maxA=%d, new_maxB=%d,new_control=%d",which,new_count,new_maxA,new_maxB,new_control);

	/* if we have a new count and we're on, update things */
	if (new_count != -1)
	{
		if (t->control & 0x8000)
		{
			internal_timer_sync(which);
			update_int_timer = 1;
		}
		t->count = new_count;
	}

	/* if we have a new max and we're on, update things */
	if (new_maxA != -1 && new_maxA != t->maxA)
	{
		if (t->control & 0x8000)
		{
			internal_timer_sync(which);
			update_int_timer = 1;
		}
		t->maxA = new_maxA;
		if (new_maxA == 0)
		{
         		new_maxA = 0x10000;
		}
	}

	/* if we have a new max and we're on, update things */
	if (new_maxB != -1 && new_maxB != t->maxB)
	{
		if (t->control & 0x8000)
		{
			internal_timer_sync(which);
			update_int_timer = 1;
		}

		t->maxB = new_maxB;

		if (new_maxB == 0)
		{
         		new_maxB = 0x10000;
      		}
   	}
    

	/* handle control changes */
	if (new_control != -1)
	{
		int diff;

		/* merge back in the bits we don't modify */
		new_control = (new_control & ~0x1fc0) | (t->control & 0x1fc0);

		/* handle the /INH bit */
		if (!(new_control & 0x4000))
			new_control = (new_control & ~0x8000) | (t->control & 0x8000);
		new_control &= ~0x4000;

		/* check for control bits we don't handle */
		diff = new_control ^ t->control;
		if (diff & 0x001c)
		  logerror("%05X:ERROR! -unsupported timer mode %04X\n",
			   cpu_get_pc(cputag_get_cpu(machine, MAINCPU_TAG)), new_control);

		/* if we have real changes, update things */
		if (diff != 0)
		{

			/* if we're going off, make sure our timers are gone */
			if ((diff & 0x8000) && !(new_control & 0x8000))
			{
				/* compute the final count */
				internal_timer_sync(which);

				/* nuke the timer and force the interrupt timer to be recomputed */
				timer_adjust_oneshot(t->time_timer, attotime_never, which);
				t->time_timer_active = 0;
				update_int_timer = 1;
			}

			/* if we're going on, start the timers running */
			else if ((diff & 0x8000) && (new_control & 0x8000))
			{
				/* start the timing */
				timer_adjust_oneshot(t->time_timer, attotime_never, which);
				t->time_timer_active = 1;
				update_int_timer = 1;
			}

			/* if something about the interrupt timer changed, force an update */
			if (!(diff & 0x8000) && (diff & 0x2000))
			{
				internal_timer_sync(which);
				update_int_timer = 1;
			}
		}

		/* set the new control register */
		t->control = new_control;
	}

	/* update the interrupt timer */
   	if (update_int_timer)
   	{
	      	if ((t->control & 0x8000) && (t->control & 0x2000))
	      	{
	        	int diff = t->maxA - t->count;
	         	if (diff <= 0)
	         		diff += 0x10000;
	         	timer_adjust_oneshot(t->int_timer, attotime_mul(ATTOTIME_IN_HZ(2000000), diff), which);
	         	if (LOG_TIMER) logerror("Set interrupt timer for %d\n", which);
	      	}
	      	else
	      	{
	        	timer_adjust_oneshot(t->int_timer, attotime_never, which);
		}
	}
}



/*************************************
 *
 *  80186 internal DMA
 *
 *************************************/

static TIMER_CALLBACK(dma_timer_callback)
{
	int which = param;
	struct dma_state *d = &i186.dma[which];

	/* force an update and see if we're really done */
	//stream_update(dma_stream, 0);

	/* complete the status update */
	d->control &= ~0x0002;
	d->source += d->count;
	d->count = 0;

	/* check for interrupt generation */
	if (d->control & 0x0100)
	{
		if (LOG_DMA) logerror("DMA%d timer callback - requesting interrupt: count = %04X, source = %04X\n", which, d->count, d->source);
		i186.intr.request |= 0x04 << which;
		update_interrupt_state(machine);
	}
}


static void update_dma_control(running_machine *machine, int which, int new_control)
{
	struct dma_state *d = &i186.dma[which];
	int diff;

	/* handle the CHG bit */
	if (!(new_control & CHG_NOCHG))
	  new_control = (new_control & ~ST_STOP) | (d->control & ST_STOP);
	new_control &= ~CHG_NOCHG;

	/* check for control bits we don't handle */
	diff = new_control ^ d->control;
	if (diff & 0x6811)
	  logerror("%05X:ERROR! - unsupported DMA mode %04X\n",
		   cpu_get_pc(cputag_get_cpu(machine, MAINCPU_TAG)), new_control);

	/* if we're going live, set a timer */
	if ((diff & 0x0002) && (new_control & 0x0002))
	{
		/* make sure the parameters meet our expectations */
		if ((new_control & 0xfe00) != 0x1600)
		{
			logerror("Unexpected DMA control %02X\n", new_control);
		}
		else if (/*!is_redline &&*/ ((d->dest & 1) || (d->dest & 0x3f) > 0x0b))
		{
			logerror("Unexpected DMA destination %02X\n", d->dest);
		}
		else if (/*is_redline && */ (d->dest & 0xf000) != 0x4000 && (d->dest & 0xf000) != 0x5000)
		{
			logerror("Unexpected DMA destination %02X\n", d->dest);
		}

		/* otherwise, set a timer */
		else
		{
//          int count = d->count;

			/* adjust for redline racer */
         	// int dacnum = (d->dest & 0x3f) / 2;

			if (LOG_DMA) logerror("Initiated DMA %d - count = %04X, source = %04X, dest = %04X\n", which, d->count, d->source, d->dest);

			d->finished = 0;
/*          timer_adjust_oneshot(d->finish_timer,
         ATTOTIME_IN_HZ(dac[dacnum].frequency) * (double)count, which);*/
		}
	}

	/* set the new control register */
	d->control = new_control;
}

static void drq_callback(running_machine *machine, int which)
{
    struct dma_state *dma = &i186.dma[which];
	const address_space *memory_space   = cpu_get_address_space(cputag_get_cpu(machine, MAINCPU_TAG), ADDRESS_SPACE_PROGRAM);
    const address_space *io_space       = cpu_get_address_space(cputag_get_cpu(machine, MAINCPU_TAG), ADDRESS_SPACE_IO);
    
    const address_space *src_space;
    const address_space *dest_space;
    
    UINT16  dma_word;
    UINT8   dma_byte;
    UINT8   incdec_size;

    if (LOG_DMA)
        logerror("Control=%04X, src=%05X, dest=%05X, count=%04X\n",dma->control,dma->source,dma->dest,dma->count);

    if(!(dma->control & ST_STOP))
    {
        logerror("%05X:ERROR! - drq%d with dma channel stopped\n",
		   cpu_get_pc(cputag_get_cpu(machine, MAINCPU_TAG)), which);
           
        return;
    }

 
    if(dma->control & DEST_MIO)
    {
        dest_space=memory_space;
        logerror("dest=memory\n");
    }
    else
    {
        dest_space=io_space;
        logerror("dest=I/O\n");
    }
    
    if(dma->control & SRC_MIO)
    {
        src_space=memory_space;
        logerror("src=memory\n");
    }
    else
    {
        src_space=io_space;
        logerror("src=I/O\n");
    }
    
    // Do the transfer
    if(dma->control & BYTE_WORD)
    {
        //logerror("moving word\n");
        dma_word=memory_read_word(src_space,dma->source);
        memory_write_word(dest_space,dma->dest,dma_word);
        incdec_size=2;
    }
    else
    {
        //logerror("moving byte\n");
        dma_byte=memory_read_byte(src_space,dma->source);
        memory_write_byte(dest_space,dma->dest,dma_byte);
        incdec_size=1;
    }
    
    // Increment or Decrement destination ans source pointers as needed
    //logerror("incrementing destination\n");
    switch (dma->control & DEST_INCDEC_MASK)
    {
        case DEST_DECREMENT     : dma->dest -= incdec_size;
        case DEST_INCREMENT     : dma->dest += incdec_size;
    }
    
    //logerror("incrementing source\n");
    switch (dma->control & SRC_INCDEC_MASK)
    {
        case SRC_DECREMENT     : dma->source -= incdec_size;
        case SRC_INCREMENT     : dma->source += incdec_size;
    }
    
    // decrement count
    dma->count -= 1;
    
    //logerror("Check term on zero\n");
    // Terminate if count is zero, and terminate flag set
    if((dma->control & TERMINATE_ON_ZERO) && (dma->count==0))
    {
        dma->control &= ~ST_STOP;
        logerror("DMA terminated\n");
    }
    
    //logerror("Check int on zero\n");
    // Interrupt if count is zero, and interrupt flag set
    if((dma->control & INTERRUPT_ON_ZERO) && (dma->count==0))
    {
		if (LOG_DMA) logerror("DMA%d - requesting interrupt: count = %04X, source = %04X\n", which, dma->count, dma->source);
		i186.intr.request |= 0x04 << which;
		update_interrupt_state(machine);      
    }
    logerror("DMA callback done\n");
}

/*-------------------------------------------------------------------------*/
/* Name: rmnimbus                                                            */
/* Desc: CPU - Initialize the 80186 CPU                                    */
/*-------------------------------------------------------------------------*/
static void nimbus_cpu_init(running_machine *machine)
{

    logerror("Machine reset\n");
    
	/* create timers here so they stick around */
	i186.timer[0].int_timer = timer_alloc(machine, internal_timer_int, NULL);
	i186.timer[1].int_timer = timer_alloc(machine, internal_timer_int, NULL);
	i186.timer[2].int_timer = timer_alloc(machine, internal_timer_int, NULL);
	i186.timer[0].time_timer = timer_alloc(machine, NULL, NULL);
	i186.timer[1].time_timer = timer_alloc(machine, NULL, NULL);
	i186.timer[2].time_timer = timer_alloc(machine, NULL, NULL);
	i186.dma[0].finish_timer = timer_alloc(machine, dma_timer_callback, NULL);
	i186.dma[1].finish_timer = timer_alloc(machine, dma_timer_callback, NULL);
}

static void nimbus_cpu_reset(running_machine *machine)
{
   	/* reset the interrupt state */
	i186.intr.priority_mask	    = 0x0007;
	i186.intr.timer 			= 0x000f;
	i186.intr.dma[0]			= 0x000f;
	i186.intr.dma[1]			= 0x000f;
	i186.intr.ext[0]			= 0x000f;
	i186.intr.ext[1]			= 0x000f;
	i186.intr.ext[2]			= 0x000f;
	i186.intr.ext[3]			= 0x000f;
    i186.intr.in_service        = 0x0000;
    
    /* External vectors by default to internal int 0/1 vectors */
    i186.intr.ext_vector[0]		= 0x000C;
	i186.intr.ext_vector[1]		= 0x000D;
	
    i186.intr.pending           = 0x0000;
	i186.intr.ack_mask          = 0x0000;
	i186.intr.request           = 0x0000;
	i186.intr.status            = 0x0000;
	i186.intr.poll_status       = 0x0000;
	
    logerror("CPU reset done\n");
}

READ16_HANDLER( i186_internal_port_r )
{
	int temp, which;

	switch (offset)
	{
		case 0x11:
			logerror("%05X:ERROR - read from 80186 EOI\n", cpu_get_pc(space->cpu));
			break;

		case 0x12:
			if (LOG_PORTS) logerror("%05X:read 80186 interrupt poll\n", cpu_get_pc(space->cpu));
			if (i186.intr.poll_status & 0x8000)
				int_callback(cputag_get_cpu(space->machine, MAINCPU_TAG), 0);
			return i186.intr.poll_status;

		case 0x13:
			if (LOG_PORTS) logerror("%05X:read 80186 interrupt poll status\n", cpu_get_pc(space->cpu));
			return i186.intr.poll_status;

		case 0x14:
			if (LOG_PORTS) logerror("%05X:read 80186 interrupt mask\n", cpu_get_pc(space->cpu));
			temp  = (i186.intr.timer  >> 3) & 0x01;
			temp |= (i186.intr.dma[0] >> 1) & 0x04;
			temp |= (i186.intr.dma[1] >> 0) & 0x08;
			temp |= (i186.intr.ext[0] << 1) & 0x10;
			temp |= (i186.intr.ext[1] << 2) & 0x20;
			temp |= (i186.intr.ext[2] << 3) & 0x40;
			temp |= (i186.intr.ext[3] << 4) & 0x80;
			return temp;

		case 0x15:
			if (LOG_PORTS) logerror("%05X:read 80186 interrupt priority mask\n", cpu_get_pc(space->cpu));
			return i186.intr.priority_mask;

		case 0x16:
			if (LOG_PORTS) logerror("%05X:read 80186 interrupt in-service\n", cpu_get_pc(space->cpu));
			return i186.intr.in_service;

		case 0x17:
			if (LOG_PORTS) logerror("%05X:read 80186 interrupt request\n", cpu_get_pc(space->cpu));
			temp = i186.intr.request & ~0x0001;
			if (i186.intr.status & 0x0007)
				temp |= 1;
			return temp;

		case 0x18:
			if (LOG_PORTS) logerror("%05X:read 80186 interrupt status\n", cpu_get_pc(space->cpu));
			return i186.intr.status;

		case 0x19:
			if (LOG_PORTS) logerror("%05X:read 80186 timer interrupt control\n", cpu_get_pc(space->cpu));
			return i186.intr.timer;

		case 0x1a:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA 0 interrupt control\n", cpu_get_pc(space->cpu));
			return i186.intr.dma[0];

		case 0x1b:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA 1 interrupt control\n", cpu_get_pc(space->cpu));
			return i186.intr.dma[1];

		case 0x1c:
			if (LOG_PORTS) logerror("%05X:read 80186 INT 0 interrupt control\n", cpu_get_pc(space->cpu));
			return i186.intr.ext[0];

		case 0x1d:
			if (LOG_PORTS) logerror("%05X:read 80186 INT 1 interrupt control\n", cpu_get_pc(space->cpu));
			return i186.intr.ext[1];

		case 0x1e:
			if (LOG_PORTS) logerror("%05X:read 80186 INT 2 interrupt control\n", cpu_get_pc(space->cpu));
			return i186.intr.ext[2];

		case 0x1f:
			if (LOG_PORTS) logerror("%05X:read 80186 INT 3 interrupt control\n", cpu_get_pc(space->cpu));
			return i186.intr.ext[3];

		case 0x28:
		case 0x2c:
		case 0x30:
			if (LOG_PORTS) logerror("%05X:read 80186 Timer %d count\n", cpu_get_pc(space->cpu), (offset - 0x28) / 4);
			which = (offset - 0x28) / 4;
			if (!(offset & 1))
				internal_timer_sync(which);
			return i186.timer[which].count;

		case 0x29:
		case 0x2d:
		case 0x31:
			if (LOG_PORTS) logerror("%05X:read 80186 Timer %d max A\n", cpu_get_pc(space->cpu), (offset - 0x29) / 4);
			which = (offset - 0x29) / 4;
			return i186.timer[which].maxA;

		case 0x2a:
		case 0x2e:
			logerror("%05X:read 80186 Timer %d max B\n", cpu_get_pc(space->cpu), (offset - 0x2a) / 4);
			which = (offset - 0x2a) / 4;
			return i186.timer[which].maxB;

		case 0x2b:
		case 0x2f:
		case 0x33:
			if (LOG_PORTS) logerror("%05X:read 80186 Timer %d control\n", cpu_get_pc(space->cpu), (offset - 0x2b) / 4);
			which = (offset - 0x2b) / 4;
			return i186.timer[which].control;

		case 0x50:
			if (LOG_PORTS) logerror("%05X:read 80186 upper chip select\n", cpu_get_pc(space->cpu));
			return i186.mem.upper;

		case 0x51:
			if (LOG_PORTS) logerror("%05X:read 80186 lower chip select\n", cpu_get_pc(space->cpu));
			return i186.mem.lower;

		case 0x52:
			if (LOG_PORTS) logerror("%05X:read 80186 peripheral chip select\n", cpu_get_pc(space->cpu));
			return i186.mem.peripheral;

		case 0x53:
			if (LOG_PORTS) logerror("%05X:read 80186 middle chip select\n", cpu_get_pc(space->cpu));
			return i186.mem.middle;

		case 0x54:
			if (LOG_PORTS) logerror("%05X:read 80186 middle P chip select\n", cpu_get_pc(space->cpu));
			return i186.mem.middle_size;

		case 0x60:
		case 0x68:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA%d lower source address\n", cpu_get_pc(space->cpu), (offset - 0x60) / 8);
			which = (offset - 0x60) / 8;
//          stream_update(dma_stream, 0);
			return i186.dma[which].source;

		case 0x61:
		case 0x69:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA%d upper source address\n", cpu_get_pc(space->cpu), (offset - 0x61) / 8);
			which = (offset - 0x61) / 8;
//          stream_update(dma_stream, 0);
			return i186.dma[which].source >> 16;

		case 0x62:
		case 0x6a:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA%d lower dest address\n", cpu_get_pc(space->cpu), (offset - 0x62) / 8);
			which = (offset - 0x62) / 8;
//          stream_update(dma_stream, 0);
			return i186.dma[which].dest;

		case 0x63:
		case 0x6b:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA%d upper dest address\n", cpu_get_pc(space->cpu), (offset - 0x63) / 8);
			which = (offset - 0x63) / 8;
//          stream_update(dma_stream, 0);
			return i186.dma[which].dest >> 16;

		case 0x64:
		case 0x6c:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA%d transfer count\n", cpu_get_pc(space->cpu), (offset - 0x64) / 8);
			which = (offset - 0x64) / 8;
//          stream_update(dma_stream, 0);
			return i186.dma[which].count;

		case 0x65:
		case 0x6d:
			if (LOG_PORTS) logerror("%05X:read 80186 DMA%d control\n", cpu_get_pc(space->cpu), (offset - 0x65) / 8);
			which = (offset - 0x65) / 8;
//          stream_update(dma_stream, 0);
			return i186.dma[which].control;

		default:
			logerror("%05X:read 80186 port %02X\n", cpu_get_pc(space->cpu), offset);
			break;
	}
	return 0x00;
}

/*************************************
 *
 *  80186 internal I/O writes
 *
 *************************************/

WRITE16_HANDLER( i186_internal_port_w )
{
	int temp, which, data16 = data;

	switch (offset)
	{
		case 0x11:
			if (LOG_PORTS) logerror("%05X:80186 EOI = %04X\n", cpu_get_pc(space->cpu), data16);
			handle_eoi(space->machine,0x8000);
			update_interrupt_state(space->machine);
			break;

		case 0x12:
			logerror("%05X:ERROR - write to 80186 interrupt poll = %04X\n", cpu_get_pc(space->cpu), data16);
			break;

		case 0x13:
			logerror("%05X:ERROR - write to 80186 interrupt poll status = %04X\n", cpu_get_pc(space->cpu), data16);
			break;

		case 0x14:
			if (LOG_PORTS) logerror("%05X:80186 interrupt mask = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.timer  = (i186.intr.timer  & ~0x08) | ((data16 << 3) & 0x08);
			i186.intr.dma[0] = (i186.intr.dma[0] & ~0x08) | ((data16 << 1) & 0x08);
			i186.intr.dma[1] = (i186.intr.dma[1] & ~0x08) | ((data16 << 0) & 0x08);
			i186.intr.ext[0] = (i186.intr.ext[0] & ~0x08) | ((data16 >> 1) & 0x08);
			i186.intr.ext[1] = (i186.intr.ext[1] & ~0x08) | ((data16 >> 2) & 0x08);
			i186.intr.ext[2] = (i186.intr.ext[2] & ~0x08) | ((data16 >> 3) & 0x08);
			i186.intr.ext[3] = (i186.intr.ext[3] & ~0x08) | ((data16 >> 4) & 0x08);
			update_interrupt_state(space->machine);
			break;

		case 0x15:
			if (LOG_PORTS) logerror("%05X:80186 interrupt priority mask = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.priority_mask = data16 & 0x0007;
			update_interrupt_state(space->machine);
			break;

		case 0x16:
			if (LOG_PORTS) logerror("%05X:80186 interrupt in-service = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.in_service = data16 & 0x00ff;
			update_interrupt_state(space->machine);
			break;

		case 0x17:
			if (LOG_PORTS) logerror("%05X:80186 interrupt request = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.request = (i186.intr.request & ~0x00c0) | (data16 & 0x00c0);
			update_interrupt_state(space->machine);
			break;

		case 0x18:
			if (LOG_PORTS) logerror("%05X:WARNING - wrote to 80186 interrupt status = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.status = (i186.intr.status & ~0x8007) | (data16 & 0x8007);
			update_interrupt_state(space->machine);
			break;

		case 0x19:
			if (LOG_PORTS) logerror("%05X:80186 timer interrupt contol = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.timer = data16 & 0x000f;
			break;

		case 0x1a:
			if (LOG_PORTS) logerror("%05X:80186 DMA 0 interrupt control = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.dma[0] = data16 & 0x000f;
			break;

		case 0x1b:
			if (LOG_PORTS) logerror("%05X:80186 DMA 1 interrupt control = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.dma[1] = data16 & 0x000f;
			break;

		case 0x1c:
			if (LOG_PORTS) logerror("%05X:80186 INT 0 interrupt control = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.ext[0] = data16 & 0x007f;
			break;

		case 0x1d:
			if (LOG_PORTS) logerror("%05X:80186 INT 1 interrupt control = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.ext[1] = data16 & 0x007f;
			break;

		case 0x1e:
			if (LOG_PORTS) logerror("%05X:80186 INT 2 interrupt control = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.ext[2] = data16 & 0x001f;
			break;

		case 0x1f:
			if (LOG_PORTS) logerror("%05X:80186 INT 3 interrupt control = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.intr.ext[3] = data16 & 0x001f;
			break;

		case 0x28:
		case 0x2c:
		case 0x30:
			if (LOG_PORTS) logerror("%05X:80186 Timer %d count = %04X\n", cpu_get_pc(space->cpu), (offset - 0x28) / 4, data16);
			which = (offset - 0x28) / 4;
			internal_timer_update(space->machine,which, data16, -1, -1, -1);
			break;

		case 0x29:
		case 0x2d:
		case 0x31:
			if (LOG_PORTS) logerror("%05X:80186 Timer %d max A = %04X\n", cpu_get_pc(space->cpu), (offset - 0x29) / 4, data16);
			which = (offset - 0x29) / 4;
			internal_timer_update(space->machine,which, -1, data16, -1, -1);
			break;

		case 0x2a:
		case 0x2e:
			if (LOG_PORTS) logerror("%05X:80186 Timer %d max B = %04X\n", cpu_get_pc(space->cpu), (offset - 0x2a) / 4, data16);
			which = (offset - 0x2a) / 4;
			internal_timer_update(space->machine,which, -1, -1, data16, -1);
			break;

		case 0x2b:
		case 0x2f:
		case 0x33:
			if (LOG_PORTS) logerror("%05X:80186 Timer %d control = %04X\n", cpu_get_pc(space->cpu), (offset - 0x2b) / 4, data16);
			which = (offset - 0x2b) / 4;
			internal_timer_update(space->machine,which, -1, -1, -1, data16);
			break;

		case 0x50:
			if (LOG_PORTS) logerror("%05X:80186 upper chip select = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.mem.upper = data16 | 0xc038;
			break;

		case 0x51:
			if (LOG_PORTS) logerror("%05X:80186 lower chip select = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.mem.lower = (data16 & 0x3fff) | 0x0038; printf("%X",i186.mem.lower);
			break;

		case 0x52:
			if (LOG_PORTS) logerror("%05X:80186 peripheral chip select = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.mem.peripheral = data16 | 0x0038;
			break;

		case 0x53:
			if (LOG_PORTS) logerror("%05X:80186 middle chip select = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.mem.middle = data16 | 0x01f8;
			break;

		case 0x54:
			if (LOG_PORTS) logerror("%05X:80186 middle P chip select = %04X\n", cpu_get_pc(space->cpu), data16);
			i186.mem.middle_size = data16 | 0x8038;

			temp = (i186.mem.peripheral & 0xffc0) << 4;
			if (i186.mem.middle_size & 0x0040)
			{
//              install_mem_read_handler(2, temp, temp + 0x2ff, peripheral_r);
//              install_mem_write_handler(2, temp, temp + 0x2ff, peripheral_w);
			}
			else
			{
				temp &= 0xffff;
//              install_port_read_handler(2, temp, temp + 0x2ff, peripheral_r);
//              install_port_write_handler(2, temp, temp + 0x2ff, peripheral_w);
			}

			/* we need to do this at a time when the I86 context is swapped in */
			/* this register is generally set once at startup and never again, so it's a good */
			/* time to set it up */
			cpu_set_irq_callback(space->cpu, int_callback);
			break;

		case 0x60:
		case 0x68:
			if (LOG_PORTS) logerror("%05X:80186 DMA%d lower source address = %04X\n", cpu_get_pc(space->cpu), (offset - 0x60) / 8, data16);
			which = (offset - 0x60) / 8;
//          stream_update(dma_stream, 0);
			i186.dma[which].source = (i186.dma[which].source & ~0x0ffff) | (data16 & 0x0ffff);
			break;

		case 0x61:
		case 0x69:
			if (LOG_PORTS) logerror("%05X:80186 DMA%d upper source address = %04X\n", cpu_get_pc(space->cpu), (offset - 0x61) / 8, data16);
			which = (offset - 0x61) / 8;
//          stream_update(dma_stream, 0);
			i186.dma[which].source = (i186.dma[which].source & ~0xf0000) | ((data16 << 16) & 0xf0000);
			break;

		case 0x62:
		case 0x6a:
			if (LOG_PORTS) logerror("%05X:80186 DMA%d lower dest address = %04X\n", cpu_get_pc(space->cpu), (offset - 0x62) / 8, data16);
			which = (offset - 0x62) / 8;
//          stream_update(dma_stream, 0);
			i186.dma[which].dest = (i186.dma[which].dest & ~0x0ffff) | (data16 & 0x0ffff);
			break;

		case 0x63:
		case 0x6b:
			if (LOG_PORTS) logerror("%05X:80186 DMA%d upper dest address = %04X\n", cpu_get_pc(space->cpu), (offset - 0x63) / 8, data16);
			which = (offset - 0x63) / 8;
//          stream_update(dma_stream, 0);
			i186.dma[which].dest = (i186.dma[which].dest & ~0xf0000) | ((data16 << 16) & 0xf0000);
			break;

		case 0x64:
		case 0x6c:
			if (LOG_PORTS) logerror("%05X:80186 DMA%d transfer count = %04X\n", cpu_get_pc(space->cpu), (offset - 0x64) / 8, data16);
			which = (offset - 0x64) / 8;
//          stream_update(dma_stream, 0);
			i186.dma[which].count = data16;
			break;

		case 0x65:
		case 0x6d:
			if (LOG_PORTS) logerror("%05X:80186 DMA%d control = %04X\n", cpu_get_pc(space->cpu), (offset - 0x65) / 8, data16);
			which = (offset - 0x65) / 8;
//          stream_update(dma_stream, 0);
			update_dma_control(space->machine, which, data16);
			break;

		case 0x7f:
			if (LOG_PORTS) logerror("%05X:80186 relocation register = %04X\n", cpu_get_pc(space->cpu), data16);

			/* we assume here there that this doesn't happen too often */
			/* plus, we can't really remove the old memory range, so we also assume that it's */
			/* okay to leave us mapped where we were */
			temp = (data16 & 0x0fff) << 8;
			if (data16 & 0x1000)
			{
				memory_install_read16_handler(cputag_get_address_space(space->machine, MAINCPU_TAG, ADDRESS_SPACE_PROGRAM), temp, temp + 0xff, 0, 0, i186_internal_port_r);
				memory_install_write16_handler(cputag_get_address_space(space->machine, MAINCPU_TAG, ADDRESS_SPACE_PROGRAM), temp, temp + 0xff, 0, 0, i186_internal_port_w);
			}
			else
			{
				temp &= 0xffff;
				memory_install_read16_handler(cputag_get_address_space(space->machine, MAINCPU_TAG, ADDRESS_SPACE_IO), temp, temp + 0xff, 0, 0, i186_internal_port_r);
				memory_install_write16_handler(cputag_get_address_space(space->machine, MAINCPU_TAG, ADDRESS_SPACE_IO), temp, temp + 0xff, 0, 0, i186_internal_port_w);
			}
/*          popmessage("Sound CPU reset");*/
			break;

		default:
			logerror("%05X:80186 port %02X = %04X\n", cpu_get_pc(space->cpu), offset, data16);
			break;
	}
}

MACHINE_RESET(nimbus)
{
	/* CPU */
	nimbus_cpu_reset(machine); 
    fdc_reset();
}

DRIVER_INIT(nimbus)
{
}

MACHINE_START( nimbus )
{
    /* init cpu */
    nimbus_cpu_init(machine);
    
	/* setup debug commands */
	if (machine->debug_flags & DEBUG_FLAG_ENABLED)
	{
		debug_console_register_command(machine, "nimbus_irq", CMDFLAG_NONE, 0, 0, 1, execute_debug_irq);
        debug_console_register_command(machine, "nimbus_intmasks", CMDFLAG_NONE, 0, 0, 0, execute_debug_intmasks);
        debug_console_register_command(machine, "nimbus_key", CMDFLAG_NONE, 0, 0, 1, execute_debug_key);
  
        
        /* set up the instruction hook */
        debug_cpu_set_instruction_hook(cputag_get_cpu(machine, MAINCPU_TAG), instruction_hook);
     
    }
}

static void execute_debug_irq(running_machine *machine, int ref, int params, const char *param[])
{
    int IntNo;
    int IntMask;
    
    if(params>0)
    {   
        sscanf(param[0],"%d",&IntNo);
        
        IntMask=0x0000;
    
        switch (IntNo)
        {
            case 0  : IntMask=0x010; break;
            case 1  : IntMask=0x020; break;
            case 2  : IntMask=0x040; break;
            case 3  : IntMask=0x080; break;
        }
        
        i186.intr.request |= IntMask;
        update_interrupt_state(machine);
        
        debug_console_printf(machine,"triggering IRQ%d, IntMask=%4.4X, i186.intr.request=%4.4X\n",IntNo,IntMask,i186.intr.request);
        
//        cputag_set_input_line(machine, MAINCPU_TAG, IntNo, HOLD_LINE );
//        cputag_set_input_line(machine, MAINCPU_TAG, IntNo, CLEAR_LINE );
        
    }
    else
    {
        debug_console_printf(machine,"Error, you must supply an intno to trigger\n");
    }
	
    
}


static void execute_debug_intmasks(running_machine *machine, int ref, int params, const char *param[])
{
    int IntNo;
    
    
    debug_console_printf(machine,"i186.intr.priority_mask=%4X\n",i186.intr.priority_mask);
    for(IntNo=0; IntNo<4; IntNo++)
    {
        debug_console_printf(machine,"extInt%d mask=%4X\n",IntNo,i186.intr.ext[IntNo]);
    }
    
    debug_console_printf(machine,"i186.intr.request   = %04X\n",i186.intr.request);
    debug_console_printf(machine,"i186.intr.ack_mask  = %04X\n",i186.intr.ack_mask);
    debug_console_printf(machine,"i186.intr.in_service= %04X\n",i186.intr.in_service);
}

static void execute_debug_key(running_machine *machine, int ref, int params, const char *param[])
{
    int Key;
    
    if(params>0)
    {   
        sscanf(param[0],"%d",&Key);
        
        nextkey=Key;
    }
    else
    {
        debug_console_printf(machine,"Error, you must supply a keycode\n");
    }
	
    
}


/*-----------------------------------------------
    instruction_hook - per-instruction hook
-----------------------------------------------*/

static int instruction_hook(const device_config *device, offs_t curpc)
{
    const address_space *space = cpu_get_address_space(device, ADDRESS_SPACE_PROGRAM);
    UINT8               *addr_ptr;
    
    addr_ptr = memory_get_read_ptr(space,curpc);
        
    if ((addr_ptr !=NULL) && (addr_ptr[0]==0xCD) && (addr_ptr[1]==0xF0))
        decode_subbios(device,curpc);
    
    return 0;
}

#define set_type(type_name)     sprintf(type_str,type_name)
#define set_drv(drv_name)       sprintf(drv_str,drv_name)
#define set_func(func_name)     sprintf(func_str,func_name)

static void decode_subbios(const device_config *device,offs_t pc)
{
    char    type_str[80];
    char    drv_str[80];
    char    func_str[80];
    
    void (*dump_dssi)(const device_config *,UINT16, UINT16) = NULL;

    const device_config *cpu = cputag_get_cpu(device->machine,MAINCPU_TAG);
    
    UINT16  ax = cpu_get_reg(cpu,I8086_AX);
    UINT16  bx = cpu_get_reg(cpu,I8086_BX);
    UINT16  cx = cpu_get_reg(cpu,I8086_CX);
    UINT16  ds = cpu_get_reg(cpu,I8086_DS);
    UINT16  si = cpu_get_reg(cpu,I8086_SI);
    
    logerror("=======================================================================\n");
    logerror("Sub-bios call at %08X, AX=%04X, BX=%04X, CX=%04X, DS:SI=%04X:%04X\n",pc,ax,bx,cx,ds,si);

    set_type("invalid");
    set_drv("invalid");
    set_func("invalid");
    

    switch (cx)
    {
        case 0   : 
        {
            set_type("t_mummu");
            set_drv("d_mummu");
            
            switch (ax)
            {
                case 0  : set_func("f_get_version_number"); break;
                case 1  : set_func("f_add_type_code"); break;
                case 2  : set_func("f_del_typc_code"); break;
                case 3  : set_func("f_get_TCB"); break;
                case 4  : set_func("f_add_driver_cdoe"); break;
                case 5  : set_func("f_del_driver_code"); break;
                case 6  : set_func("f_get_DCB"); break;
                case 7  : set_func("f_get_copyright"); break;
            }
        }; break;
        
        case 1   : 
        {
            set_type("t_character");
            set_drv("d_printer");
            
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;
                case 1  : set_func("f_get_output_status"); break;
                case 2  : set_func("f_output_character"); break;
                case 3  : set_func("f_get_input_status"); break;
                case 4  : set_func("f_get_and_remove"); break;
                case 5  : set_func("f_get_no_remove"); break;
                case 6  : set_func("f_get_last_and_remove"); break;
                case 7  : set_func("f_get_last_no_remove"); break;
                case 8  : set_func("f_set_IO_parameters"); break;
            }
        }; break;
        
        case 2   : 
        {
            set_type("t_disk");
            
            switch(bx)
            {
                case 0  : set_drv("d_floppy"); break;
                case 1  : set_drv("d_winchester"); break;
                case 2  : set_drv("d_tape"); break;
                case 3  : set_drv("d_rompack"); break;
                case 4  : set_drv("d_eeprom"); break;
            }
            
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;            
                case 1  : set_func("f_initialise_unit"); break;
                case 2  : set_func("f_pseudo_init_unit"); break;
                case 3  : set_func("f_get_device_status"); break;
                case 4  : set_func("f_read_n_sectors"); break;
                case 5  : set_func("f_write_n_sectors"); break;
                case 6  : set_func("f_verify_n_sectors"); break;
                case 7  : set_func("f_media_check"); break;
                case 8  : set_func("f_recalibrate"); break;
                case 9  : set_func("f_motors_off"); break;
            }
            
        }; break;
        
        case 3   : 
        {
            set_type("t_piconet");
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;            
            }
        }; break;
        
        case 4   : 
        {
            set_type("t_tick");
            set_drv("d_tick");
            
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break; 
                case 1  : set_func("f_ticks_per_second"); break; 
                case 2  : set_func("f_link_tick_routine"); break; 
                case 3  : set_func("f_unlink_tick_routine"); break;            
            }
        }; break;
        
        case 5   : 
        {
            set_type("t_graphics_input");
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;            
            }
        }; break;
        
        case 6   : 
        {
            set_type("t_graphics_output");
            set_drv("d_ngc_screen");

            switch(ax)
            {
                case 0  : set_func("f_get_version_number");                 break; 
                case 1  : set_func("f_graphics_output_cold_start");         break; 
                case 2  : set_func("f_graphics_output_warm_start");         break; 
                case 3  : set_func("f_graphics_output_off");                break; 
                case 4  : set_func("f_reinit_graphics_output");             break; 
                case 5  : set_func("f_polymarker");                         break; 
                case 6  : set_func("f_polyline");                           break; 
                case 7  : set_func("f_fill_area"); dump_dssi=&decode_dssi_f_fill_area; break; 
                case 8  : set_func("f_flood_fill_area"); break; 
                case 9  : set_func("f_plot_character_string"); dump_dssi=&decode_dssi_f_plot_character_string; break; 
                case 10 : set_func("f_define_graphics_clipping_area"); break; 
                case 11 : set_func("f_enquire_clipping_area_limits"); break; 
                case 12 : set_func("f_select_graphics_clipping_area"); break; 
                case 13 : set_func("f_enq_selctd_graphics_clip_area"); break; 
                case 14 : set_func("f_set_clt_element"); break; 
                case 15 : set_func("f_enquire_clt_element"); break; 
                case 16 : set_func("f_set_new_clt"); dump_dssi=&decode_dssi_f_set_new_clt; break; 
                case 17 : set_func("f_enquire_clt_contents"); break; 
                case 18 : set_func("f_define_dithering_pattern"); break; 
                case 19 : set_func("f_enquire_dithering_pattern"); break; 
                case 20 : set_func("f_draw_sprite"); break; 
                case 21 : set_func("f_move_sprite"); break; 
                case 22 : set_func("f_erase_sprite"); break; 
                case 23 : set_func("f_read_pixel"); break; 
                case 24 : set_func("f_read_to_limit"); break; 
                case 25 : set_func("f_read_area_pixel"); break; 
                case 26 : set_func("f_write_area_pixel"); break; 
                case 27 : set_func("f_copy_area_pixel"); break; 

                case 29 : set_func("f_read_area_word"); break; 
                case 30 : set_func("f_write_area_word"); break; 
                case 31 : set_func("f_copy_area_word"); break; 
                case 32 : set_func("f_swap_area_word"); break; 
                case 33 : set_func("f_set_border_colour"); break; 
                case 34 : set_func("f_enquire_border_colour"); break; 
                case 35 : set_func("f_enquire_miscellaneous_data"); break; 
                case 36  : set_func("f_circle"); break; 

                case 38 : set_func("f_arc_of_ellipse"); break; 
                case 39 : set_func("f_isin"); break; 
                case 40 : set_func("f_icos"); break; 
                case 41 : set_func("f_define_hatching_pattern"); break; 
                case 42 : set_func("f_enquire_hatching_pattern"); break; 
                case 43 : set_func("f_enquire_display_line"); break; 
                case 44 : set_func("f_plonk_logo"); break; 
            }
        }; break;
        
        case 7   : 
        {
            set_type("t_zend");
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;            
            }
        }; break;
        
        case 8   : 
        {
            set_type("t_zep");
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;            
            }
        }; break;
        
        case 9   : 
        {
            set_type("t_raw_console");
            
            switch(bx)
            {
                case 0  : 
                {
                    set_drv("d_screen");
            
                    switch(ax)
                    {
                        case 0  : set_func("f_get_version_number"); break;            
                        case 1  : set_func("f_plonk_char"); break;
                        case 2  : set_func("f_plonk_cursor"); break;
                        case 3  : set_func("f_kill_cursor"); break;
                        case 4  : set_func("f_scroll"); break;
                        case 5  : set_func("f_width"); break;
                        case 6  : set_func("f_get_char_set"); break;
                        case 7  : set_func("f_set_char_set"); break;
                        case 8  : set_func("f_reset_char_set"); break;
                        case 9  : set_func("f_set_plonk_parameters"); break;
                        case 10 : set_func("f_set_cursor_flash_rate"); break;
                    }
                }; break;
            
                case 1  :
                {
                    set_drv("d_keyboard");
                    
                    switch(ax)
                    {
                        case 0  : set_func("f_get_version_number"); break;            
                        case 1  : set_func("f_init_keyboard"); break;
                        case 2  : set_func("f_get_last_key_code"); break;
                        case 3  : set_func("f_get_bitmap"); break;
                    }
                }; break;
            }
        }; break;
        
        case 10   : 
        {
            set_type("t_acoustics");
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;            
            }
        }; break;
        
        case 11   : 
        {
            set_type("t_hard_sums");
            switch(ax)
            {
                case 0  : set_func("f_get_version_number"); break;            
            }
        }; break;    
    }
    
    logerror("Type=%s, Driver=%s, Function=%s\n",type_str,drv_str,func_str);
    
    if(dump_dssi!=NULL)
        dump_dssi(device,ds,si);
    logerror("=======================================================================\n");
}

void *get_dssi_ptr(const address_space *space, UINT16   ds, UINT16 si)
{
    int             addr;

    addr=((ds<<4)+si);
    OUTPUT_SEGOFS("DS:SI",ds,si);

    return memory_get_read_ptr(space, addr);
}

static void decode_dssi_f_fill_area(const device_config *device,UINT16  ds, UINT16 si)
{
    const address_space *space = cputag_get_address_space(device->machine,MAINCPU_TAG, ADDRESS_SPACE_PROGRAM);
 
    UINT16          *addr_ptr;
    t_area_params   *area_params;
    t_nimbus_brush  *brush;
    int             cocount;

    area_params = get_dssi_ptr(space,ds,si);
    
    OUTPUT_SEGOFS("SegBrush:OfsBrush",area_params->seg_brush,area_params->ofs_brush);
    brush=memory_get_read_ptr(space, LINEAR_ADDR(area_params->seg_brush,area_params->ofs_brush));
    
    logerror("Brush params\n");
    logerror("Style=%04X,          StyleIndex=%04X\n",brush->style,brush->style_index);
    logerror("Colour1=%04X,        Colour2=%04X\n",brush->colour1,brush->colour2);
    logerror("transparency=%04X,   boundry_spec=%04X\n",brush->transparency,brush->boundary_spec);
    logerror("boundry colour=%04X, save colour=%04X\n",brush->boundary_colour,brush->save_colour);
            
    
    OUTPUT_SEGOFS("SegData:OfsData",area_params->seg_data,area_params->ofs_data);
    
    addr_ptr = memory_get_read_ptr(space, LINEAR_ADDR(area_params->seg_data,area_params->ofs_data));
    for(cocount=0; cocount < area_params->count; cocount++)
    {
        logerror("x=%d y=%d\n",addr_ptr[cocount*2],addr_ptr[(cocount*2)+1]);
    }
}

static void decode_dssi_f_plot_character_string(const device_config *device,UINT16  ds, UINT16 si)
{
    const address_space *space = cputag_get_address_space(device->machine,MAINCPU_TAG, ADDRESS_SPACE_PROGRAM);
 
    UINT8          *char_ptr;
    t_plot_string_params   *plot_string_params;
    int             charno;
       
    plot_string_params=get_dssi_ptr(space,ds,si);
    
    OUTPUT_SEGOFS("SegFont:OfsFont",plot_string_params->seg_font,plot_string_params->ofs_font);
    OUTPUT_SEGOFS("SegData:OfsData",plot_string_params->seg_data,plot_string_params->ofs_data);
    
    logerror("x=%d, y=%d, length=%d\n",plot_string_params->x,plot_string_params->y,plot_string_params->length);
    
    char_ptr=memory_get_read_ptr(space, LINEAR_ADDR(plot_string_params->seg_data,plot_string_params->ofs_data));

    if (plot_string_params->length==0xFFFF)
        logerror("%s",char_ptr);
    else
        for(charno=0;charno<plot_string_params->length;charno++)
            logerror("%c",char_ptr[charno]);
        
    logerror("\n");
}

static void decode_dssi_f_set_new_clt(const device_config *device,UINT16  ds, UINT16 si)
{
    const address_space *space = cputag_get_address_space(device->machine,MAINCPU_TAG, ADDRESS_SPACE_PROGRAM);
    UINT16  *new_colours;
    int     colour;
    new_colours=get_dssi_ptr(space,ds,si);
    
    OUTPUT_SEGOFS("SegColours:OfsColours",ds,si);
    
    for(colour=0;colour<16;colour++)
        logerror("colour #%02X=%04X\n",colour,new_colours[colour]);
    
}

READ16_HANDLER( nimbus_io_r )
{
    int pc=cpu_get_pc(space->cpu);
    
    logerror("Nimbus IOR at pc=%08X from %04X mask=%04X, data=%04X\n",pc,(offset*2)+0x30,mem_mask,IOPorts[offset]);
    
    switch (offset*2)
    {
        default         : return IOPorts[offset]; break;
    }
    return 0;
}

WRITE16_HANDLER( nimbus_io_w )
{
    int pc=cpu_get_pc(space->cpu);

    logerror("Nimbus IOW at %08X write of %04X to %04X mask=%04X\n",pc,data,(offset*2)+0x30,mem_mask);
    
    switch (offset*2)
    {
        default         : COMBINE_DATA(&IOPorts[offset]); break;
    }
}

/* 

Z80SIO, used for the keyboard interface 

*/

READ8_DEVICE_HANDLER( sio_r )
{
    int pc=cpu_get_pc(cputag_get_cpu(device->machine,MAINCPU_TAG));
    UINT8 result = 0;
    
	switch (offset*2)
	{
        case 0 :
            result=z80sio_d_r(device, 0); 
            break;
        case 2 :
            result=z80sio_d_r(device, 1); 
            break;
        case 4 :
            result=z80sio_c_r(device, 0);
            break;
        case 6 :
            result=z80sio_c_r(device, 1);
            break;
	}

    logerror("Nimbus SIOR at pc=%08X from %04X data=%02X\n",pc,(offset*2)+0xF0,result);

	return result;
}

WRITE8_DEVICE_HANDLER( sio_w )
{
    int pc=cpu_get_pc(cputag_get_cpu(device->machine,MAINCPU_TAG));

    logerror("Nimbus SIOW at %08X write of %02X to %04X\n",pc,data,(offset*2)+0xF0);

	switch (offset*2)
	{
        case 0 :
            z80sio_d_w(device, 0, data);
            break;
        case 2 :
            z80sio_d_w(device, 1, data);
            break;
        case 4 :
            z80sio_c_w(device, 0, data);
            break;
        case 5 :
            z80sio_c_w(device, 1, data);
            break;
	}
}

/* Z80 SIO/2 */

void sio_interrupt(const device_config *device, int state)
{
    logerror("SIO Interrupt state=%02X\n",state);

    // Don't re-trigger if already active !
    if(state!=sio_int_state)
    {
        sio_int_state=state;
        
        if(state)
            external_int(device->machine,0,0x9C);
    }
}

WRITE8_DEVICE_HANDLER( sio_dtr_w )
{
	if (offset == 1)
	{
	}
}

WRITE8_DEVICE_HANDLER( sio_serial_transmit )
{
}

int sio_serial_receive( const device_config *device, int channel )
{
    UINT8   keyrow;
    UINT8   row;
    static const char *const keynames[] = { "KEY0", "KEY1", "KEY2", "KEY3", "KEY4",
                                             "KEY5", "KEY6", "KEY7", "KEY8", "KEY9" };
    int     key;

	if(channel==0)
    {
        key=-1;
        
        //logerror("Read keyboard\n");
        for(row=0;row<NIMBUS_KEYROWS;row++)
        {
            keyrow=input_port_read(device->machine, keynames[row]);
            if(keyrows[row]!=keyrow)
            {
                keyrows[row]=keyrow;
                key=~keyrow & 0xff;
            }
        }
        
        if (nextkey!=0)
        {
            key=nextkey;
            nextkey=0;
        }
        
        if(key!=-1) 
            logerror("Keypressed=%02X\n",key);
        
        return key;
    }
    else
        return -1;
}

/* Floppy disk */

static void fdc_reset(void)
{
    nimbus_fdc.irq_enabled=0;
    nimbus_fdc.drq_enabled=0;
    nimbus_fdc.diskno=0;
    nimbus_fdc.motor_on=0;
}

static WRITE_LINE_DEVICE_HANDLER( nimbus_fdc_intrq_w )
{
	logerror("nimbus_fdc_intrq_w(%d)\n", state);

    if(state && nimbus_fdc.irq_enabled)
    {
        external_int(device->machine,0,0x80);
    } 
}

static WRITE_LINE_DEVICE_HANDLER( nimbus_fdc_drq_w )
{
	logerror("nimbus_fdc_drq_w(%d)\n", state);
    
    if(state && nimbus_fdc.drq_enabled)
        drq_callback(device->machine,1);
}

READ8_HANDLER( nimbus_fdc_r )
{
   	int result = 0;
	const device_config *fdc = devtag_get_device(space->machine, FDC_TAG);
    int pc=cpu_get_pc(space->cpu);
    const device_config *drive = devtag_get_device(space->machine, nimbus_wd17xx_interface.floppy_drive_tags[nimbus_fdc.diskno]);
	
    switch(offset*2)
	{
		case 0x08 :
			result = wd17xx_status_r(fdc, 0);
			logerror("Disk status=%2.2X\n",result);
			break;
		case 0x0A :
			result = wd17xx_track_r(fdc, 0);
			break;
		case 0x0C :
			result = wd17xx_sector_r(fdc, 0);
			break;
		case 0x0E :
			result = wd17xx_data_r(fdc, 0);
			break;
        case 0x10 :
            result = (nimbus_fdc.motor_on                                    ? 0x04 : 0x00) |
                     (floppy_drive_get_flag_state(drive, FLOPPY_DRIVE_INDEX) ? 0x00 : 0x02) |
                     (floppy_drive_get_flag_state(drive, FLOPPY_DRIVE_READY) ? 0x01 : 0x00);      
            break;
		default:
			break;
	}

    logerror("Nimbus FDCR at pc=%08X from %04X data=%02X\n",pc,(offset*2)+0x400,result);

	return result;
}

WRITE8_HANDLER( nimbus_fdc_w )
{
	const device_config *fdc = devtag_get_device(space->machine, FDC_TAG);
    int pc=cpu_get_pc(space->cpu);

    logerror("Nimbus FDCW at %08X write of %02X to %04X\n",pc,data,(offset*2)+0x400);

    switch(offset*2)
	{
        case 0x00 :
            switch (data & 0x0F)
            {
                case 0x01 : nimbus_fdc.diskno=0; break;
                case 0x02 : nimbus_fdc.diskno=1; break;
                case 0x04 : nimbus_fdc.diskno=2; break;
                case 0x08 : nimbus_fdc.diskno=3; break;  
                //default : nimbus_fdc.diskno=NO_DRIVE_SELECTED; break; // no drive selected
            }
            if (nimbus_fdc.diskno!=NO_DRIVE_SELECTED)
            {
                wd17xx_set_drive(fdc, nimbus_fdc.diskno);
                wd17xx_set_side(fdc, (data & 0x10) ? 1 : 0);
            }
            nimbus_fdc.motor_on=(data & 0x20);
            nimbus_fdc.drq_enabled=(data & 0x80);
            nimbus_fdc.irq_enabled=1;
            
            // Nimbus FDC is hard wired for double density
            wd17xx_set_density(fdc, DEN_MFM_LO);
			break;
		case 0x08 :
			wd17xx_command_w(fdc, 0, data);
			break;
		case 0x0A :
			wd17xx_track_w(fdc, 0, data);
			break;
		case 0x0C :
			wd17xx_sector_w(fdc, 0, data);
			break;
		case 0x0E :
			wd17xx_data_w(fdc, 0, data);
			break;
	}
}

