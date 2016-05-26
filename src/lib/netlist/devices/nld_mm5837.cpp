// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_MM5837.c
 *
 */

#include <solver/nld_matrix_solver.h>
#include "nld_mm5837.h"
#include "nl_setup.h"

#define R_LOW (1000)
#define R_HIGH (1000)

NETLIB_NAMESPACE_DEVICES_START()

NETLIB_RESET(MM5837_dip)
{
	m_V0.initial(0.0);
	m_RV.do_reset();
	m_RV.set(NL_FCONST(1.0) / R_LOW, 0.0, 0.0);

	m_shift = 0x1ffff;
	m_is_timestep = m_RV.m_P.net().solver()->is_timestep();
}

NETLIB_UPDATE(MM5837_dip)
{
	OUTLOGIC(m_Q, !m_Q.net().as_logic().new_Q(), m_inc  );

	/* shift register
	 *
	 * 17 bits, bits 17 & 14 feed back to input
	 *
	 */

	const UINT32 last_state = m_shift & 0x01;
	/* shift */
	m_shift = (m_shift >> 1) | (((m_shift & 0x01) ^ ((m_shift >> 3) & 0x01)) << 16);
	const UINT32 state = m_shift & 0x01;

	if (state != last_state)
	{
		const nl_double R = state ? R_HIGH : R_LOW;
		const nl_double V = state ? INPANALOG(m_VDD) : INPANALOG(m_VSS);

		// We only need to update the net first if this is a time stepping net
		if (m_is_timestep)
			m_RV.update_dev();
		m_RV.set(NL_FCONST(1.0) / R, V, 0.0);
		m_RV.m_P.schedule_after(NLTIME_FROM_NS(1));
	}

}



NETLIB_NAMESPACE_DEVICES_END()
