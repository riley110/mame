// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nld_twoterm.h
 *
 * Devices with two terminals ...
 *
 *
 *       (k)
 *  +-----T-----+
 *  |     |     |
 *  |  +--+--+  |
 *  |  |     |  |
 *  |  R     |  |
 *  |  R     |  |
 *  |  R     I  |
 *  |  |     I  |  Device n
 *  |  V+    I  |
 *  |  V     |  |
 *  |  V-    |  |
 *  |  |     |  |
 *  |  +--+--+  |
 *  |     |     |
 *  +-----T-----+
 *       (l)
 *
 *  This is a resistance in series to a voltage source and paralleled by a
 *  current source. This is suitable to model voltage sources, current sources,
 *  resistors, capacitors, inductances and diodes.
 *
 */

#ifndef NLD_TWOTERM_H_
#define NLD_TWOTERM_H_

#include "nl_base.h"

// -----------------------------------------------------------------------------
// Macros
// -----------------------------------------------------------------------------

#define RES(_name, _R)                                                         \
		NET_REGISTER_DEV(RES, _name)                                           \
		NETDEV_PARAMI(_name, R, _R)

#define POT(_name, _R)                                                         \
		NET_REGISTER_DEV(POT, _name)                                           \
		NETDEV_PARAMI(_name, R, _R)

/* Does not have pin 3 connected */
#define POT2(_name, _R)                                                        \
		NET_REGISTER_DEV(POT2, _name)                                          \
		NETDEV_PARAMI(_name, R, _R)


#define CAP(_name, _C)                                                         \
		NET_REGISTER_DEV(CAP, _name)                                           \
		NETDEV_PARAMI(_name, C, _C)

/* Generic Diode */
#define DIODE(_name,  _model)                                                  \
		NET_REGISTER_DEV(DIODE, _name)                                         \
		NETDEV_PARAMI(_name, MODEL, _model)

#define VS(_name, _V)                                                          \
		NET_REGISTER_DEV(VS, _name)                                            \
		NETDEV_PARAMI(_name, V, _V)

#define CS(_name, _I)                                                          \
		NET_REGISTER_DEV(CS, _name)                                            \
		NETDEV_PARAMI(_name, I, _I)

// -----------------------------------------------------------------------------
// Generic macros
// -----------------------------------------------------------------------------


#ifdef RES_R
// FIXME: avoid compile fails
// #warning "Do not include rescap.h in a netlist environment"
#endif

#define RES_R(res) ((double)(res))
#define RES_K(res) ((double)(res) * 1e3)
#define RES_M(res) ((double)(res) * 1e6)
#define CAP_U(cap) ((double)(cap) * 1e-6)
#define CAP_N(cap) ((double)(cap) * 1e-9)
#define CAP_P(cap) ((double)(cap) * 1e-12)
#define IND_U(ind) ((double)(ind) * 1e-6)
#define IND_N(ind) ((double)(ind) * 1e-9)
#define IND_P(ind) ((double)(ind) * 1e-12)

// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------

NETLIB_NAMESPACE_DEVICES_START()

// -----------------------------------------------------------------------------
// nld_twoterm
// -----------------------------------------------------------------------------

NETLIB_OBJECT(twoterm)
{
public:
	NETLIB_CONSTRUCTOR(twoterm)
	{
		m_P.m_otherterm = &m_N;
		m_N.m_otherterm = &m_P;
	}

	terminal_t m_P;
	terminal_t m_N;

	NETLIB_UPDATE_TERMINALSI()
	{
	}

	ATTR_HOT /* inline */ void set(const nl_double G, const nl_double V, const nl_double I)
	{
		/*      GO, GT, I                */
		m_P.set( G,  G, (  V) * G - I);
		m_N.set( G,  G, ( -V) * G + I);
	}

	ATTR_HOT /* inline */ nl_double deltaV() const
	{
		return m_P.net().as_analog().Q_Analog() - m_N.net().as_analog().Q_Analog();
	}

	ATTR_HOT void set_mat(nl_double a11, nl_double a12, nl_double a21, nl_double a22, nl_double r1, nl_double r2)
	{
		/*      GO, GT, I                */
		m_P.set(-a12, a11, -r1);
		m_N.set(-a21, a22, -r2);
	}

protected:
	ATTR_HOT virtual void start() override;
	ATTR_HOT virtual void reset() override;
	ATTR_HOT void update() override;

private:
};


// -----------------------------------------------------------------------------
// nld_R
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(R_base, twoterm)
{
	NETLIB_CONSTRUCTOR_DERIVED(R_base, twoterm)
	{
		enregister("1", m_P);
		enregister("2", m_N);
	}

public:
	inline void set_R(const nl_double R)
	{
		set(NL_FCONST(1.0) / R, 0.0, 0.0);
	}

protected:
	NETLIB_RESETI()
	{
		NETLIB_NAME(twoterm)::reset();
		set_R(1.0 / netlist().gmin());
	}

	NETLIB_UPDATEI()
	{
		NETLIB_NAME(twoterm)::update();
	}


};

NETLIB_OBJECT_DERIVED(R, R_base)
{
	NETLIB_CONSTRUCTOR_DERIVED(R, R_base)
	{
		register_param("R", m_R, 1.0 / netlist().gmin());
	}

	param_double_t m_R;

protected:

	NETLIB_RESETI() { NETLIB_NAME(R_base)::reset(); }
	NETLIB_UPDATEI() { NETLIB_NAME(twoterm)::update(); }
	NETLIB_UPDATE_PARAMI()
	{
		update_dev();
		if (m_R.Value() > 1e-9)
			set_R(m_R.Value());
		else
			set_R(1e-9);
	}

};

// -----------------------------------------------------------------------------
// nld_POT
// -----------------------------------------------------------------------------

NETLIB_OBJECT(POT)
{
	NETLIB_CONSTRUCTOR(POT)
	, m_R1(*this, "R1")
	, m_R2(*this, "R2")
	{
		register_subalias("1", m_R1.m_P);
		register_subalias("2", m_R1.m_N);
		register_subalias("3", m_R2.m_N);

		connect_late(m_R2.m_P, m_R1.m_N);

		register_param("R", m_R, 1.0 / netlist().gmin());
		register_param("DIAL", m_Dial, 0.5);
		register_param("DIALLOG", m_DialIsLog, 0);
	}

	NETLIB_UPDATEI();
	NETLIB_RESETI();
	NETLIB_UPDATE_PARAMI();

private:
	NETLIB_SUB(R_base) m_R1;
	NETLIB_SUB(R_base) m_R2;

	param_double_t m_R;
	param_double_t m_Dial;
	param_logic_t m_DialIsLog;
};

NETLIB_OBJECT(POT2)
{
	NETLIB_CONSTRUCTOR(POT2)
	, m_R1(*this, "R1")
	{
		register_subalias("1", m_R1.m_P);
		register_subalias("2", m_R1.m_N);

		register_param("R", m_R, 1.0 / netlist().gmin());
		register_param("DIAL", m_Dial, 0.5);
		register_param("REVERSE", m_Reverse, 0);
		register_param("DIALLOG", m_DialIsLog, 0);
	}

	NETLIB_UPDATEI();
	NETLIB_RESETI();
	NETLIB_UPDATE_PARAMI();

private:
	NETLIB_SUB(R_base) m_R1;

	param_double_t m_R;
	param_double_t m_Dial;
	param_logic_t m_DialIsLog;
	param_logic_t m_Reverse;
};


// -----------------------------------------------------------------------------
// nld_C
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(C, twoterm)
{
public:
	NETLIB_CONSTRUCTOR_DERIVED(C, twoterm)
	, m_GParallel(0.0)
	{ }

	NETLIB_TIMESTEP()
	{
		/* Gpar should support convergence */
		const nl_double G = m_C.Value() / step +  m_GParallel;
		const nl_double I = -G * deltaV();
		set(G, 0.0, I);
	}


	param_double_t m_C;

protected:
	virtual void start() override;
	virtual void reset() override;
	virtual void update_param() override;
	ATTR_HOT void update() override;

private:
	nl_double m_GParallel;

};


// -----------------------------------------------------------------------------
// A generic diode model to be used in other devices (Diode, BJT ...)
// -----------------------------------------------------------------------------

class generic_diode
{
public:
	ATTR_COLD generic_diode();

	ATTR_HOT inline void update_diode(const nl_double nVd)
	{
#if 1
		if (nVd < NL_FCONST(-5.0) * m_Vt)
		{
			m_Vd = nVd;
			m_G = m_gmin;
			m_Id = - m_Is;
		}
		else if (nVd < m_Vcrit)
		{
			m_Vd = nVd;
			//m_Vd = m_Vd + 10.0 * m_Vt * std::tanh((nVd - m_Vd) / 10.0 / m_Vt);
			const nl_double eVDVt = nl_math::exp(m_Vd * m_VtInv);
			m_Id = m_Is * (eVDVt - NL_FCONST(1.0));
			m_G = m_Is * m_VtInv * eVDVt + m_gmin;
		}
		else
		{
#if 1
			const nl_double a = std::max((nVd - m_Vd) * m_VtInv, NL_FCONST(0.5) - NL_FCONST(1.0));
			m_Vd = m_Vd + nl_math::e_log1p(a) * m_Vt;
#else
			m_Vd = m_Vd + 10.0 * m_Vt * std::tanh((nVd - m_Vd) / 10.0 / m_Vt);
#endif
			const nl_double eVDVt = nl_math::exp(m_Vd * m_VtInv);
			m_Id = m_Is * (eVDVt - NL_FCONST(1.0));

			m_G = m_Is * m_VtInv * eVDVt + m_gmin;
		}
#else
		m_Vd = m_Vd + 20.0 * m_Vt * std::tanh((nVd - m_Vd) / 20.0 / m_Vt);
		const nl_double eVDVt = nl_math::exp(m_Vd * m_VtInv);
		m_Id = m_Is * (eVDVt - NL_FCONST(1.0));
		m_G = m_Is * m_VtInv * eVDVt + m_gmin;
#endif
	}

	ATTR_COLD void set_param(const nl_double Is, const nl_double n, nl_double gmin);

	ATTR_HOT inline nl_double I() const { return m_Id; }
	ATTR_HOT inline nl_double G() const { return m_G; }
	ATTR_HOT inline nl_double Ieq() const { return (m_Id - m_Vd * m_G); }
	ATTR_HOT inline nl_double Vd() const { return m_Vd; }

	/* owning object must save those ... */

	ATTR_COLD void save(pstring name, object_t &parent);

private:
	nl_double m_Vd;
	nl_double m_Id;
	nl_double m_G;

	nl_double m_Vt;
	nl_double m_Is;
	nl_double m_n;
	nl_double m_gmin;

	nl_double m_VtInv;
	nl_double m_Vcrit;
};

// -----------------------------------------------------------------------------
// nld_D
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(D, twoterm)
{
public:
	NETLIB_CONSTRUCTOR_DERIVED(D, twoterm)
	{ }

	NETLIB_DYNAMIC()

	NETLIB_UPDATE_TERMINALSI();

	param_model_t m_model;

protected:
	virtual void start() override;
	virtual void update_param() override;
	ATTR_HOT void update() override;

	generic_diode m_D;
};


// -----------------------------------------------------------------------------
// nld_VS - Voltage source
//
// netlist voltage source must have inner resistance
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(VS, twoterm)
{
public:
	NETLIB_CONSTRUCTOR_DERIVED(VS, twoterm) { }

protected:
	virtual void start() override;
	virtual void reset() override;
	ATTR_HOT void update() override;

	param_double_t m_R;
	param_double_t m_V;
};

// -----------------------------------------------------------------------------
// nld_CS - Current source
// -----------------------------------------------------------------------------

NETLIB_OBJECT_DERIVED(CS, twoterm)
{
public:
	NETLIB_CONSTRUCTOR_DERIVED(CS, twoterm) { }

protected:
	virtual void start() override;
	virtual void reset() override;
	ATTR_HOT void update() override;

	param_double_t m_I;
};


NETLIB_NAMESPACE_DEVICES_END()

#endif /* NLD_TWOTERM_H_ */
