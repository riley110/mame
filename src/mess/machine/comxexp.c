/**********************************************************************

    COMX-35 expansion bus emulation

    Copyright MESS Team.
    Visit http://mamedev.org for licensing and usage restrictions.

**********************************************************************/

#include "emu.h"
#include "emuopts.h"
#include "machine/comxexp.h"


//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

const device_type COMX_EXPANSION_SLOT = &device_creator<comx_expansion_slot_device>;



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  comx_expansion_slot_device - constructor
//-------------------------------------------------
comx_expansion_slot_device::comx_expansion_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
        device_t(mconfig, COMX_EXPANSION_SLOT, "COMX-35 expansion slot", tag, owner, clock),
		device_slot_interface(mconfig, *this)
{
}

void comx_expansion_slot_device::static_set_slot(device_t &device, const char *tag, int num)
{
	comx_expansion_slot_device &comx_expansion_card = dynamic_cast<comx_expansion_slot_device &>(device);
	comx_expansion_card.m_bus_tag = tag;
	comx_expansion_card.m_bus_num = num;
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void comx_expansion_slot_device::device_start()
{
	m_bus = machine().device<comx_expansion_bus_device>(m_bus_tag);
	device_comx_expansion_card_interface *dev = dynamic_cast<device_comx_expansion_card_interface *>(get_card_device());
	if (dev) m_bus->add_card(dev, m_bus_num);
}



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

const device_type COMX_EXPANSION_BUS = &device_creator<comx_expansion_bus_device>;


void comx_expansion_bus_device::static_set_cputag(device_t &device, const char *tag)
{
	comx_expansion_bus_device &comx_expansion = downcast<comx_expansion_bus_device &>(device);
	comx_expansion.m_cputag = tag;
}


//-------------------------------------------------
//  device_config_complete - perform any
//  operations now that the configuration is
//  complete
//-------------------------------------------------

void comx_expansion_bus_device::device_config_complete()
{
	// inherit a copy of the static data
	const comx_expansion_bus_interface *intf = reinterpret_cast<const comx_expansion_bus_interface *>(static_config());
	if (intf != NULL)
	{
		*static_cast<comx_expansion_bus_interface *>(this) = *intf;
	}

	// or initialize to defaults if none provided
	else
	{
    	memset(&m_out_int_cb, 0, sizeof(m_out_int_cb));
    	memset(&m_out_ef4_cb, 0, sizeof(m_out_ef4_cb));
    	memset(&m_out_wait_cb, 0, sizeof(m_out_wait_cb));
    	memset(&m_out_clear_cb, 0, sizeof(m_out_clear_cb));
    	memset(&m_out_extrom_cb, 0, sizeof(m_out_extrom_cb));
	}
}



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  comx_expansion_bus_device - constructor
//-------------------------------------------------

comx_expansion_bus_device::comx_expansion_bus_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
        device_t(mconfig, COMX_EXPANSION_BUS, "COMX_EXPANSION", tag, owner, clock)
{
	for (int i = 0; i < MAX_COMX_EXPANSION_SLOTS; i++)
		m_comx_expansion_bus_device[i] = NULL;
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void comx_expansion_bus_device::device_start()
{
	m_maincpu = machine().device(m_cputag);

	// resolve callbacks
	m_out_int_func.resolve(m_out_int_cb, *this);
	m_out_ef4_func.resolve(m_out_ef4_cb, *this);
	m_out_wait_func.resolve(m_out_wait_cb, *this);
	m_out_clear_func.resolve(m_out_clear_cb, *this);
	m_out_extrom_func.resolve(m_out_extrom_cb, *this);
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void comx_expansion_bus_device::device_reset()
{
}


//-------------------------------------------------
//  add_card - add COMX_EXPANSION card
//-------------------------------------------------

void comx_expansion_bus_device::add_card(device_comx_expansion_card_interface *card, int pos)
{
	m_comx_expansion_bus_device[pos] = card;
}


//-------------------------------------------------
//  mrd_r - memory read
//-------------------------------------------------

READ8_MEMBER( comx_expansion_bus_device::mrd_r )
{
	UINT8 data = 0;

	for (int i = 0; i < MAX_COMX_EXPANSION_SLOTS; i++)
	{
		if (m_comx_expansion_bus_device[i] != NULL)
		{
			data |= m_comx_expansion_bus_device[i]->comx_mrd_r(offset);
		}
	}

	return data;
}


//-------------------------------------------------
//  mwr_w - memory write
//-------------------------------------------------

WRITE8_MEMBER( comx_expansion_bus_device::mwr_w )
{
	for (int i = 0; i < MAX_COMX_EXPANSION_SLOTS; i++)
	{
		if (m_comx_expansion_bus_device[i] != NULL)
		{
			m_comx_expansion_bus_device[i]->comx_mwr_w(offset, data);
		}
	}
}


//-------------------------------------------------
//  io_r - I/O read
//-------------------------------------------------

READ8_MEMBER( comx_expansion_bus_device::io_r )
{
	UINT8 data = 0;

	for (int i = 0; i < MAX_COMX_EXPANSION_SLOTS; i++)
	{
		if (m_comx_expansion_bus_device[i] != NULL)
		{
			data |= m_comx_expansion_bus_device[i]->comx_io_r(offset);
		}
	}

	return data;
}


//-------------------------------------------------
//  sout_w - I/O write
//-------------------------------------------------

WRITE8_MEMBER( comx_expansion_bus_device::io_w )
{
	for (int i = 0; i < MAX_COMX_EXPANSION_SLOTS; i++)
	{
		if (m_comx_expansion_bus_device[i] != NULL)
		{
			m_comx_expansion_bus_device[i]->comx_io_w(offset, data);
		}
	}
}


//-------------------------------------------------
//  q_w - Q write
//-------------------------------------------------

WRITE_LINE_MEMBER( comx_expansion_bus_device::q_w )
{
	for (int i = 0; i < MAX_COMX_EXPANSION_SLOTS; i++)
	{
		if (m_comx_expansion_bus_device[i] != NULL)
		{
			m_comx_expansion_bus_device[i]->comx_q_w(state);
		}
	}
}


WRITE_LINE_MEMBER( comx_expansion_bus_device::int_w ) { m_out_int_func(state); }
WRITE_LINE_MEMBER( comx_expansion_bus_device::ef4_w ) { m_out_ef4_func(state); }
WRITE_LINE_MEMBER( comx_expansion_bus_device::wait_w ) { m_out_wait_func(state); }
WRITE_LINE_MEMBER( comx_expansion_bus_device::clear_w ) { m_out_clear_func(state); }
WRITE_LINE_MEMBER( comx_expansion_bus_device::extrom_w ) { m_out_extrom_func(state); }



//**************************************************************************
//  DEVICE COMX_EXPANSION CARD INTERFACE
//**************************************************************************

//-------------------------------------------------
//  device_comx_expansion_card_interface - constructor
//-------------------------------------------------

device_comx_expansion_card_interface::device_comx_expansion_card_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device)
{
}


//-------------------------------------------------
//  ~device_comx_expansion_card_interface - destructor
//-------------------------------------------------

device_comx_expansion_card_interface::~device_comx_expansion_card_interface()
{
}
