// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_system.h
 *
 * netlist devices defined in the core
 */

#ifndef NLD_SYSTEM_H_
#define NLD_SYSTEM_H_

#include "nl_setup.h"
#include "nl_base.h"
#include "nl_factory.h"
#include "analog/nld_twoterm.h"

// -----------------------------------------------------------------------------
// Macros
// -----------------------------------------------------------------------------

#define TTL_INPUT(_name, _v)                                                   \
		NET_REGISTER_DEV(TTL_INPUT, _name)                                   \
		PARAM(_name.IN, _v)

#define LOGIC_INPUT(_name, _v, _family)                                        \
		NET_REGISTER_DEV(LOGIC_INPUT, _name)                                   \
		PARAM(_name.IN, _v)                                                    \
		PARAM(_name.FAMILY, _family)

#define ANALOG_INPUT(_name, _v)                                                \
		NET_REGISTER_DEV(ANALOG_INPUT, _name)                                  \
		PARAM(_name.IN, _v)

#define MAINCLOCK(_name, _freq)                                                \
		NET_REGISTER_DEV(MAINCLOCK, _name)                                     \
		PARAM(_name.FREQ, _freq)

#define CLOCK(_name, _freq)                                                    \
		NET_REGISTER_DEV(CLOCK, _name)                                         \
		PARAM(_name.FREQ, _freq)

#define EXTCLOCK(_name, _freq, _pattern)                                       \
		NET_REGISTER_DEV(EXTCLOCK, _name)                                      \
		PARAM(_name.FREQ, _freq)                                               \
		PARAM(_name.PATTERN, _pattern)

#define GNDA()                                                                 \
		NET_REGISTER_DEV(GNDA, GND)

#define DUMMY_INPUT(_name)                                                     \
		NET_REGISTER_DEV(DUMMY_INPUT, _name)

//FIXME: Usage discouraged, use OPTIMIZE_FRONTIER instead
#define FRONTIER_DEV(_name, _IN, _G, _OUT)                                     \
		NET_REGISTER_DEV(FRONTIER_DEV, _name)                                      \
		NET_C(_IN, _name.I)                                                    \
		NET_C(_G,  _name.G)                                                    \
		NET_C(_OUT, _name.Q)

#define OPTIMIZE_FRONTIER(_attach, _r_in, _r_out)                              \
		setup.register_frontier(# _attach, _r_in, _r_out);

#define RES_SWITCH(_name, _IN, _P1, _P2)                                       \
		NET_REGISTER_DEV(RES_SWITCH, _name)                                        \
		NET_C(_IN, _name.I)                                                    \
		NET_C(_P1, _name.1)                                                    \
		NET_C(_P2, _name.2)

/* Default device to hold netlist parameters */
#define PARAMETERS(_name)                                                      \
		NET_REGISTER_DEV(PARAMETERS, _name)

#define AFUNC(_name, _N, _F)                                                   \
		NET_REGISTER_DEV(AFUNC, _name)                                      \
		PARAM(_name.N, _N)                                                     \
		PARAM(_name.FUNC, _F)

NETLIB_NAMESPACE_DEVICES_START()

// -----------------------------------------------------------------------------
// netlistparams
// -----------------------------------------------------------------------------

NETLIB_OBJECT(netlistparams)
{
	NETLIB_CONSTRUCTOR(netlistparams)
	{
		register_param("USE_DEACTIVATE", m_use_deactivate, 0);
	}
	NETLIB_UPDATEI() { }
	//NETLIB_RESETI() { }
	NETLIB_UPDATE_PARAMI() { }
public:
	param_logic_t m_use_deactivate;
};

// -----------------------------------------------------------------------------
// mainclock
// -----------------------------------------------------------------------------

NETLIB_OBJECT(mainclock)
{
	NETLIB_CONSTRUCTOR(mainclock)
	{
		enregister("Q", m_Q);

		register_param("FREQ", m_freq, 7159000.0 * 5);
		m_inc = netlist_time::from_hz(m_freq.Value()*2);
	}

	NETLIB_RESETI()
	{
		m_Q.net().set_time(netlist_time::zero);
	}

	NETLIB_UPDATE_PARAMI()
	{
		m_inc = netlist_time::from_hz(m_freq.Value()*2);
	}

	NETLIB_UPDATEI()
	{
		logic_net_t &net = m_Q.net().as_logic();
		// this is only called during setup ...
		net.toggle_new_Q();
		net.set_time(netlist().time() + m_inc);
	}

public:
	logic_output_t m_Q;

	param_double_t m_freq;
	netlist_time m_inc;

	ATTR_HOT inline static void mc_update(logic_net_t &net);
};

// -----------------------------------------------------------------------------
// clock
// -----------------------------------------------------------------------------

NETLIB_OBJECT(clock)
{
	NETLIB_CONSTRUCTOR(clock)
	{
		enregister("Q", m_Q);
		enregister("FB", m_feedback);

		register_param("FREQ", m_freq, 7159000.0 * 5.0);
		m_inc = netlist_time::from_hz(m_freq.Value()*2);

		connect_late(m_feedback, m_Q);
	}
	NETLIB_UPDATEI();
	//NETLIB_RESETI();
	NETLIB_UPDATE_PARAMI();

protected:
	logic_input_t m_feedback;
	logic_output_t m_Q;

	param_double_t m_freq;
	netlist_time m_inc;
};

// -----------------------------------------------------------------------------
// extclock
// -----------------------------------------------------------------------------

NETLIB_OBJECT(extclock)
{
	NETLIB_CONSTRUCTOR(extclock)
	{
		enregister("Q", m_Q);
		enregister("FB", m_feedback);

		register_param("FREQ", m_freq, 7159000.0 * 5.0);
		register_param("PATTERN", m_pattern, "1,1");
		register_param("OFFSET", m_offset, 0.0);
		m_inc[0] = netlist_time::from_hz(m_freq.Value()*2);

		connect_late(m_feedback, m_Q);
		{
			netlist_time base = netlist_time::from_hz(m_freq.Value()*2);
			pstring_vector_t pat(m_pattern.Value(),",");
			m_off = netlist_time::from_double(m_offset.Value());

			int pati[256];
			m_size = pat.size();
			int total = 0;
			for (int i=0; i<m_size; i++)
			{
				pati[i] = pat[i].as_long();
				total += pati[i];
			}
			netlist_time ttotal = netlist_time::zero;
			for (int i=0; i<m_size - 1; i++)
			{
				m_inc[i] = base * pati[i];
				ttotal += m_inc[i];
			}
			m_inc[m_size - 1] = base * total - ttotal;
		}
		save(NLNAME(m_cnt));
		save(NLNAME(m_off));
	}
	NETLIB_UPDATEI();
	NETLIB_RESETI();
	//NETLIB_UPDATE_PARAMI();
protected:

	param_double_t m_freq;
	param_str_t m_pattern;
	param_double_t m_offset;

	logic_input_t m_feedback;
	logic_output_t m_Q;
	UINT32 m_cnt;
	UINT32 m_size;
	netlist_time m_inc[32];
	netlist_time m_off;
};

// -----------------------------------------------------------------------------
// Special support devices ...
// -----------------------------------------------------------------------------

NETLIB_OBJECT(logic_input)
{
	NETLIB_CONSTRUCTOR(logic_input)
	{
		/* make sure we get the family first */
		register_param("FAMILY", m_FAMILY, "FAMILY(TYPE=TTL)");
		set_logic_family(netlist().setup().family_from_model(m_FAMILY.Value()));

		enregister("Q", m_Q);
		register_param("IN", m_IN, 0);
	}
	NETLIB_UPDATEI();
	//NETLIB_RESETI();
	NETLIB_UPDATE_PARAMI();

protected:
	logic_output_t m_Q;

	param_logic_t m_IN;
	param_model_t m_FAMILY;
};

NETLIB_OBJECT(analog_input)
{
	NETLIB_CONSTRUCTOR(analog_input)
	{
		enregister("Q", m_Q);
		register_param("IN", m_IN, 0.0);
	}
	NETLIB_UPDATEI();
	//NETLIB_RESETI();
	NETLIB_UPDATE_PARAMI();
protected:
	analog_output_t m_Q;
	param_double_t m_IN;
};

// -----------------------------------------------------------------------------
// nld_gnd
// -----------------------------------------------------------------------------

NETLIB_OBJECT(gnd)
{
	NETLIB_CONSTRUCTOR(gnd)
	{
		enregister("Q", m_Q);
	}
	NETLIB_UPDATEI()
	{
		OUTANALOG(m_Q, 0.0);
	}
	NETLIB_RESETI() { }
protected:
	analog_output_t m_Q;
};

// -----------------------------------------------------------------------------
// nld_dummy_input
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(dummy_input, base_dummy)
{
public:
	NETLIB_CONSTRUCTOR_DERIVED(dummy_input, base_dummy)
	{
		enregister("I", m_I);
	}

protected:

	NETLIB_RESETI() { }
	NETLIB_UPDATEI() { }

private:
	analog_input_t m_I;

};

// -----------------------------------------------------------------------------
// nld_frontier
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(frontier, base_dummy)
{
public:
	NETLIB_CONSTRUCTOR_DERIVED(frontier, base_dummy)
	, m_RIN(netlist(), "m_RIN")
	, m_ROUT(netlist(), "m_ROUT")
	{
		register_param("RIN", m_p_RIN, 1.0e6);
		register_param("ROUT", m_p_ROUT, 50.0);

		enregister("_I", m_I);
		enregister("I",m_RIN.m_P);
		enregister("G",m_RIN.m_N);
		connect_late(m_I, m_RIN.m_P);

		enregister("_Q", m_Q);
		enregister("_OP",m_ROUT.m_P);
		enregister("Q",m_ROUT.m_N);
		connect_late(m_Q, m_ROUT.m_P);
	}

	NETLIB_RESETI()
	{
		m_RIN.set(1.0 / m_p_RIN.Value(),0,0);
		m_ROUT.set(1.0 / m_p_ROUT.Value(),0,0);
	}

	NETLIB_UPDATEI()
	{
		OUTANALOG(m_Q, INPANALOG(m_I));
	}

private:
	NETLIB_NAME(twoterm) m_RIN;
	NETLIB_NAME(twoterm) m_ROUT;
	analog_input_t m_I;
	analog_output_t m_Q;

	param_double_t m_p_RIN;
	param_double_t m_p_ROUT;
};

/* -----------------------------------------------------------------------------
 * nld_function
 *
 * FIXME: Currently a proof of concept to get congo bongo working
 * ----------------------------------------------------------------------------- */

NETLIB_OBJECT(function)
{
	NETLIB_CONSTRUCTOR(function)
	{
		register_param("N", m_N, 2);
		register_param("FUNC", m_func, "");
		enregister("Q", m_Q);

		for (int i=0; i < m_N; i++)
			enregister(pfmt("A{1}")(i), m_I[i]);

		pstring_vector_t cmds(m_func.Value(), " ");
		m_precompiled.clear();

		for (std::size_t i=0; i < cmds.size(); i++)
		{
			pstring cmd = cmds[i];
			rpn_inst rc;
			if (cmd == "+")
				rc.m_cmd = ADD;
			else if (cmd == "-")
				rc.m_cmd = SUB;
			else if (cmd == "*")
				rc.m_cmd = MULT;
			else if (cmd == "/")
				rc.m_cmd = DIV;
			else if (cmd.startsWith("A"))
			{
				rc.m_cmd = PUSH_INPUT;
				rc.m_param = cmd.substr(1).as_long();
			}
			else
			{
				bool err = false;
				rc.m_cmd = PUSH_CONST;
				rc.m_param = cmd.as_double(&err);
				if (err)
					netlist().log().fatal("nld_function: unknown/misformatted token <{1}> in <{2}>", cmd, m_func.Value());
			}
			m_precompiled.push_back(rc);
		}


	}

protected:

	NETLIB_RESETI();
	NETLIB_UPDATEI();

private:

	enum rpn_cmd
	{
		ADD,
		MULT,
		SUB,
		DIV,
		PUSH_CONST,
		PUSH_INPUT
	};

	struct rpn_inst
	{
		rpn_inst() : m_cmd(ADD), m_param(0.0) { }
		rpn_cmd m_cmd;
		nl_double m_param;
	};

	param_int_t m_N;
	param_str_t m_func;
	analog_output_t m_Q;
	analog_input_t m_I[10];

	pvector_t<rpn_inst> m_precompiled;
};

// -----------------------------------------------------------------------------
// nld_res_sw
// -----------------------------------------------------------------------------

NETLIB_OBJECT(res_sw)
{
public:
	NETLIB_CONSTRUCTOR(res_sw)
	, m_R(*this, "R")
	{
		enregister("I", m_I);
		register_param("RON", m_RON, 1.0);
		register_param("ROFF", m_ROFF, 1.0E20);

		register_subalias("1", m_R.m_P);
		register_subalias("2", m_R.m_N);

		save(NLNAME(m_last_state));
	}

	NETLIB_SUB(R) m_R;
	param_double_t m_RON;
	param_double_t m_ROFF;
	logic_input_t m_I;

protected:

	NETLIB_RESETI()
	{
		m_last_state = 0;
		m_R.set_R(m_ROFF.Value());
	}
	NETLIB_UPDATE_PARAMI();
	NETLIB_UPDATEI();

private:
	UINT8 m_last_state;
};

// -----------------------------------------------------------------------------
// nld_base_proxy
// -----------------------------------------------------------------------------

NETLIB_OBJECT(base_proxy)
{
public:
	nld_base_proxy(netlist_t &anetlist, const pstring &name, logic_t *inout_proxied, core_terminal_t *proxy_inout)
			: device_t(anetlist, name)
	{
		m_logic_family = inout_proxied->logic_family();
		m_term_proxied = inout_proxied;
		m_proxy_term = proxy_inout;
	}

	virtual ~nld_base_proxy() {}

	logic_t &term_proxied() const { return *m_term_proxied; }
	core_terminal_t &proxy_term() const { return *m_proxy_term; }

protected:

	virtual const logic_family_desc_t &logic_family() const
	{
		return *m_logic_family;
	}

private:
	const logic_family_desc_t *m_logic_family;
	logic_t *m_term_proxied;
	core_terminal_t *m_proxy_term;
};

// -----------------------------------------------------------------------------
// nld_a_to_d_proxy
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(a_to_d_proxy, base_proxy)
{
public:
	nld_a_to_d_proxy(netlist_t &anetlist, const pstring &name, logic_input_t *in_proxied)
			: nld_base_proxy(anetlist, name, in_proxied, &m_I)
	{
		enregister("I", m_I);
		enregister("Q", m_Q);
	}

	virtual ~nld_a_to_d_proxy() {}

	analog_input_t m_I;
	logic_output_t m_Q;

protected:

	NETLIB_RESETI() { }

	NETLIB_UPDATEI()
	{
		if (m_I.Q_Analog() > logic_family().m_high_thresh_V)
			OUTLOGIC(m_Q, 1, NLTIME_FROM_NS(1));
		else if (m_I.Q_Analog() < logic_family().m_low_thresh_V)
			OUTLOGIC(m_Q, 0, NLTIME_FROM_NS(1));
		else
		{
			// do nothing
		}
	}
private:
};

// -----------------------------------------------------------------------------
// nld_base_d_to_a_proxy
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(base_d_to_a_proxy, base_proxy)
{
public:
	virtual ~nld_base_d_to_a_proxy() {}

	virtual logic_input_t &in() { return m_I; }

protected:
	nld_base_d_to_a_proxy(netlist_t &anetlist, const pstring &name, logic_output_t *out_proxied, core_terminal_t &proxy_out)
			: nld_base_proxy(anetlist, name, out_proxied, &proxy_out)
	{
		enregister("I", m_I);
	}

	logic_input_t m_I;

private:
};

NETLIB_OBJECT_DERIVED(d_to_a_proxy, base_d_to_a_proxy)
{
public:
	nld_d_to_a_proxy(netlist_t &anetlist, const pstring &name, logic_output_t *out_proxied)
	: nld_base_d_to_a_proxy(anetlist, name, out_proxied, m_RV.m_P)
	, m_RV(*this, "RV")
	, m_last_state(-1)
	, m_is_timestep(false)
	{
		//register_sub(m_RV);
		enregister("1", m_RV.m_P);
		enregister("2", m_RV.m_N);

		enregister("_Q", m_Q);
		register_subalias("Q", m_RV.m_P);

		connect_late(m_RV.m_N, m_Q);

		save(NLNAME(m_last_state));
	}

	virtual ~nld_d_to_a_proxy() {}

protected:

	NETLIB_RESETI();
	NETLIB_UPDATEI();

private:
	analog_output_t m_Q;
	NETLIB_SUB(twoterm) m_RV;
	int m_last_state;
	bool m_is_timestep;
};


class factory_lib_entry_t : public base_factory_t
{
	P_PREVENT_COPYING(factory_lib_entry_t)
public:

	ATTR_COLD factory_lib_entry_t(setup_t &setup, const pstring &name, const pstring &classname,
			const pstring &def_param)
	: base_factory_t(name, classname, def_param), m_setup(setup) {  }

	class wrapper : public device_t
	{
	public:
		wrapper(const pstring &dev_name, netlist_t &anetlist, const pstring &name)
		: device_t(anetlist, name), m_dev_name(dev_name)
		{
			anetlist.setup().namespace_push(name);
			anetlist.setup().include(m_dev_name);
			anetlist.setup().namespace_pop();
		}
	protected:
		NETLIB_RESETI() { }
		NETLIB_UPDATEI() { }

		pstring m_dev_name;
	};

	powned_ptr<device_t> Create(netlist_t &anetlist, const pstring &name) override
	{
		return powned_ptr<device_t>::Create<wrapper>(this->name(), anetlist, name);
	}

private:
	setup_t &m_setup;
};


NETLIB_NAMESPACE_DEVICES_END()

#endif /* NLD_SYSTEM_H_ */
