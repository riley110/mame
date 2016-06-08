// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_truthtable.c
 *
 */

#include "nld_truthtable.h"
#include "plib/plists.h"

namespace netlist
{
	namespace devices
	{

	template<unsigned m_NI, unsigned m_NO, int has_state>
	class netlist_factory_truthtable_t : public netlist_base_factory_truthtable_t
	{
		P_PREVENT_COPYING(netlist_factory_truthtable_t)
	public:
		netlist_factory_truthtable_t(const pstring &name, const pstring &classname,
				const pstring &def_param)
		: netlist_base_factory_truthtable_t(name, classname, def_param)
		{ }

		plib::owned_ptr<device_t> Create(netlist_t &anetlist, const pstring &name) override
		{
			typedef nld_truthtable_t<m_NI, m_NO, has_state> tt_type;
			return plib::owned_ptr<device_t>::Create<tt_type>(anetlist, name, m_family, &m_ttbl, m_desc);
		}
	private:
		typename nld_truthtable_t<m_NI, m_NO, has_state>::truthtable_t m_ttbl;
	};

	static const uint64_t all_set = ~((uint64_t) 0);

unsigned truthtable_desc_t::count_bits(uint64_t v)
{
	uint64_t ret = 0;
	for (; v != 0; v = v >> 1)
	{
		ret += (v & 1);
	}
	return ret;
}

uint64_t truthtable_desc_t::set_bits(uint64_t v, uint64_t b)
{
	uint64_t ret = 0;
	for (size_t i = 0; v != 0; v = v >> 1, ++i)
	{
		if (v & 1)
		{
			ret |= ((b&1)<<i);
			b = b >> 1;
		}
	}
	return ret;
}

uint64_t truthtable_desc_t::get_ignored_simple(uint64_t i)
{
	uint64_t m_enable = 0;
	for (size_t j=0; j<m_size; j++)
	{
		if (m_outs[j] != m_outs[i])
		{
			m_enable |= (i ^ j);
		}
	}
	return m_enable ^ (m_size - 1);
}

uint64_t truthtable_desc_t::get_ignored_extended(uint64_t state)
{
	// Determine all inputs which may be ignored ...
	uint64_t ignore = 0;
	for (unsigned j=0; j<m_NI; j++)
	{
		if (m_outs[state] == m_outs[state ^ (1 << j)])
			ignore |= (1<<j);
	}
	/* Check all permutations of ign
	 * We have to remove those where the ignored inputs
	 * may change the output
	 */
	uint64_t bits = (1<<count_bits(ignore));
	std::vector<bool> t(bits);

	for (size_t j=1; j<bits; j++)
	{
		uint64_t tign = set_bits(ignore, j);
		t[j] = 0;
		uint64_t bitsk=(1<<count_bits(tign));
		for (size_t k=0; k<bitsk; k++)
		{
			uint64_t b=set_bits(tign, k);
			if (m_outs[state] != m_outs[(state & ~tign) | b])
			{
				t[j] = 1;
				break;
			}
		}
	}
	size_t jb=0;
	size_t jm=0;
	for (UINT32 j=1; j<bits; j++)
	{
		size_t nb = count_bits(j);
		if ((t[j] == 0) && (nb>jb))
		{
			jb = nb;
			jm = j;
		}
	}
	return set_bits(ignore, jm);
}

// ----------------------------------------------------------------------------------------
// desc
// ----------------------------------------------------------------------------------------

void truthtable_desc_t::help(unsigned cur, plib::pstring_vector_t list,
		uint64_t state, uint64_t val, std::vector<uint8_t> &timing_index)
{
	pstring elem = list[cur].trim();
	int start = 0;
	int end = 0;

	if (elem.equals("0"))
	{
		start = 0;
		end = 0;
	}
	else if (elem.equals("1"))
	{
		start = 1;
		end = 1;
	}
	else if (elem.equals("X"))
	{
		start = 0;
		end = 1;
	}
	else
		nl_assert_always(false, "unknown input value (not 0, 1, or X)");
	for (int i = start; i <= end; i++)
	{
		const UINT64 nstate = state | (i << cur);

		if (cur < m_num_bits - 1)
		{
			help(cur + 1, list, nstate, val, timing_index);
		}
		else
		{
			// cutoff previous inputs and outputs for ignore
			if (m_outs[nstate] != m_outs.adjust(all_set) &&  m_outs[nstate] != val)
				fatalerror_e(plib::pfmt("Error in truthtable: State {1} already set, {2} != {3}\n")
						.x(nstate,"04")(m_outs[nstate])(val) );
			m_outs.set(nstate, val);
			for (unsigned j=0; j<m_NO; j++)
				m_timing[nstate * m_NO + j] = timing_index[j];
		}
	}
}

void truthtable_desc_t::setup(const plib::pstring_vector_t &truthtable, UINT32 disabled_ignore)
{
	unsigned line = 0;

	if (*m_initialized)
		return;

	pstring ttline = truthtable[line];
	line++;
	ttline = truthtable[line];
	line++;

	for (unsigned j=0; j < m_size; j++)
		m_outs.set(j, all_set);

	for (int j=0; j < 16; j++)
		m_timing_nt[j] = netlist_time::zero();

	while (!ttline.equals(""))
	{
		plib::pstring_vector_t io(ttline,"|");
		// checks
		nl_assert_always(io.size() == 3, "io.count mismatch");
		plib::pstring_vector_t inout(io[0], ",");
		nl_assert_always(inout.size() == m_num_bits, "number of bits not matching");
		plib::pstring_vector_t out(io[1], ",");
		nl_assert_always(out.size() == m_NO, "output count not matching");
		plib::pstring_vector_t times(io[2], ",");
		nl_assert_always(times.size() == m_NO, "timing count not matching");

		UINT16 val = 0;
		std::vector<UINT8> tindex;

		for (unsigned j=0; j<m_NO; j++)
		{
			pstring outs = out[j].trim();
			if (outs.equals("1"))
				val = val | (1 << j);
			else
				nl_assert_always(outs.equals("0"), "Unknown value (not 0 or 1");
			netlist_time t = netlist_time::from_nsec(times[j].trim().as_long());
			int k=0;
			while (m_timing_nt[k] != netlist_time::zero() && m_timing_nt[k] != t)
				k++;
			m_timing_nt[k] = t;
			tindex.push_back(k); //[j] = k;
		}

		help(0, inout, 0 , val, tindex);
		if (line < truthtable.size())
			ttline = truthtable[line];
		else
			ttline = "";
		line++;
	}

	// determine ignore
	std::vector<uint64_t> ign(m_size, all_set);

	for (size_t i=0; i<m_size; i++)
	{
		if (ign[i] == all_set)
		{
			int tign;
			if (0)
			{
				tign = get_ignored_simple(i);
				ign[i] = tign;
			}
			else
			{
				tign = get_ignored_extended(i);

				ign[i] = tign;
				/* don't need to recalculate similar ones */
				uint64_t bitsk=(1<<count_bits(tign));
				for (uint64_t k=0; k<bitsk; k++)
				{
					uint64_t b=set_bits(tign, k);
					ign[(i & ~tign) | b] = tign;
				}
			}
		}
	}
	for (size_t i=0; i<m_size; i++)
	{
		if (m_outs[i] == m_outs.adjust(all_set))
			throw fatalerror_e(plib::pfmt("truthtable: found element not set {1}\n").x(i) );
		m_outs.set(i, m_outs[i] | ((ign[i] & ~disabled_ignore)  << m_NO));;
	}
	*m_initialized = true;

}

#define ENTRYX(n, m, h)    case (n * 1000 + m * 10 + h): \
	{ using xtype = netlist_factory_truthtable_t<n, m, h>; \
		return plib::owned_ptr<netlist_base_factory_truthtable_t>::Create<xtype>(name,classname,def_param); } break

#define ENTRYY(n, m)   ENTRYX(n, m, 0); ENTRYX(n, m, 1)

#define ENTRY(n) ENTRYY(n, 1); ENTRYY(n, 2); ENTRYY(n, 3); ENTRYY(n, 4); ENTRYY(n, 5); ENTRYY(n, 6)

plib::owned_ptr<netlist_base_factory_truthtable_t> nl_tt_factory_create(const unsigned ni, const unsigned no,
		const unsigned has_state,
		const pstring &name, const pstring &classname,
		const pstring &def_param)
{
	switch (ni * 1000 + no * 10 + has_state)
	{
		ENTRY(1);
		ENTRY(2);
		ENTRY(3);
		ENTRY(4);
		ENTRY(5);
		ENTRY(6);
		ENTRY(7);
		ENTRY(8);
		ENTRY(9);
		ENTRY(10);
		default:
			pstring msg = plib::pfmt("unable to create truthtable<{1},{2},{3}>")(ni)(no)(has_state);
			nl_assert_always(false, msg);
	}
	//return nullptr;
}

	} //namespace devices
} // namespace netlist
