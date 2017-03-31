// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria
#include "machine/namco06.h"
#include "sound/samples.h"

class galaga_hbmame : public galaga_state
{
public:
	galaga_hbmame(const machine_config &mconfig, device_type type, const char *tag)
		: galaga_state(mconfig, type, tag)
		, m_samples(*this, "samples")
		, m_06xx(*this, "06xx")
		{ }

	DECLARE_WRITE8_MEMBER(galaga_sample_w);

private:
	optional_device<samples_device> m_samples;
	optional_device<namco_06xx_device> m_06xx;
};
