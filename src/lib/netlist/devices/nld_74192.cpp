// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_74192.c
 *
 */

#define MAXCNT 9

#include "nld_74192.h"

namespace netlist
{
	namespace devices
	{

	NETLIB_OBJECT(74192_subABCD)
	{
		NETLIB_CONSTRUCTOR(74192_subABCD)
		, m_A(*this, "A")
		, m_B(*this, "B")
		, m_C(*this, "C")
		, m_D(*this, "D")
		{
		}

		//NETLIB_RESETI()
		//NETLIB_UPDATEI();

	public:
		logic_input_t m_A;
		logic_input_t m_B;
		logic_input_t m_C;
		logic_input_t m_D;

		inline UINT8 read_ABCD() const
		{
			//return (INPLOGIC_PASSIVE(m_D) << 3) | (INPLOGIC_PASSIVE(m_C) << 2) | (INPLOGIC_PASSIVE(m_B) << 1) | (INPLOGIC_PASSIVE(m_A) << 0);
			return (INPLOGIC(m_D) << 3) | (INPLOGIC(m_C) << 2) | (INPLOGIC(m_B) << 1) | (INPLOGIC(m_A) << 0);
		}
	};

	NETLIB_OBJECT(74192)
	{
		NETLIB_CONSTRUCTOR(74192)
		, m_ABCD(*this, "subABCD")
		, m_CLEAR(*this, "CLEAR")
		, m_LOADQ(*this, "LOADQ")
		, m_CU(*this, "CU")
		, m_CD(*this, "CD")
		, m_Q(*this, {{"QA", "QB", "QC", "QD"}})
		, m_BORROWQ(*this, "BORROWQ")
		, m_CARRYQ(*this, "CARRYQ")
		{
			register_subalias("A", m_ABCD.m_A);
			register_subalias("B", m_ABCD.m_B);
			register_subalias("C", m_ABCD.m_C);
			register_subalias("D", m_ABCD.m_D);

			save(NLNAME(m_cnt));
			save(NLNAME(m_last_CU));
			save(NLNAME(m_last_CD));
		}

		NETLIB_RESETI();
		NETLIB_UPDATEI();

	protected:
		NETLIB_SUB(74192_subABCD) m_ABCD;
		logic_input_t m_CLEAR;
		logic_input_t m_LOADQ;
		logic_input_t m_CU;
		logic_input_t m_CD;

		INT8 m_cnt;
		UINT8 m_last_CU;
		UINT8 m_last_CD;

		object_array_t<logic_output_t, 4> m_Q;
		logic_output_t m_BORROWQ;
		logic_output_t m_CARRYQ;
	};

	NETLIB_OBJECT_DERIVED(74192_dip, 74192)
	{
		NETLIB_CONSTRUCTOR_DERIVED(74192_dip, 74192)
		{
			register_subalias("1", m_ABCD.m_B);
			register_subalias("2", m_Q[1]);
			register_subalias("3", m_Q[0]);
			register_subalias("4", m_CD);
			register_subalias("5", m_CU);
			register_subalias("6", m_Q[2]);
			register_subalias("7", m_Q[3]);

			register_subalias("9", m_ABCD.m_D);
			register_subalias("10", m_ABCD.m_C);
			register_subalias("11", m_LOADQ);
			register_subalias("12", m_CARRYQ);
			register_subalias("13", m_BORROWQ);
			register_subalias("14", m_CLEAR);
			register_subalias("15", m_ABCD.m_A);
		}
	};

	NETLIB_RESET(74192)
	{
		m_cnt = 0;
		m_last_CU = 0;
		m_last_CD = 0;
	}

	// FIXME: Timing
	static const netlist_time delay[4] =
	{
			NLTIME_FROM_NS(40),
			NLTIME_FROM_NS(40),
			NLTIME_FROM_NS(40),
			NLTIME_FROM_NS(40)
	};

	NETLIB_UPDATE(74192)
	{
		int tCarry = 1;
		int tBorrow = 1;
		if (INPLOGIC(m_CLEAR))
		{
			m_cnt = 0;
		}
		else if (!INPLOGIC(m_LOADQ))
		{
			m_cnt = m_ABCD.read_ABCD();
		}
		else
		{
			if (INPLOGIC(m_CD) && !m_last_CU && INPLOGIC(m_CU))
			{
				m_cnt++;
				if (m_cnt > MAXCNT)
					m_cnt = 0;
			}
			if (INPLOGIC(m_CU) && !m_last_CD && INPLOGIC(m_CD))
			{
				if (m_cnt > 0)
					m_cnt--;
				else
					m_cnt = MAXCNT;
			}
		}

		if (!INPLOGIC(m_CU) && (m_cnt == MAXCNT))
			tCarry = 0;

		if (!INPLOGIC(m_CD) && (m_cnt == 0))
			tBorrow = 0;

		m_last_CD = INPLOGIC(m_CD);
		m_last_CU = INPLOGIC(m_CU);

		for (int i=0; i<4; i++)
			OUTLOGIC(m_Q[i], (m_cnt >> i) & 1, delay[i]);

		OUTLOGIC(m_BORROWQ, tBorrow, NLTIME_FROM_NS(20)); //FIXME
		OUTLOGIC(m_CARRYQ, tCarry, NLTIME_FROM_NS(20)); //FIXME
	}

	NETLIB_DEVICE_IMPL(74192)
	NETLIB_DEVICE_IMPL(74192_dip)

	} //namespace devices
} // namespace netlist
