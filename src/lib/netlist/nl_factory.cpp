// license:GPL-2.0+
// copyright-holders:Couriersud
/***************************************************************************

    nl_factory.c

    Discrete netlist implementation.

****************************************************************************/

#include "nl_factory.h"
#include "nl_setup.h"

namespace netlist
{
// ----------------------------------------------------------------------------------------
// net_device_t_base_factory
// ----------------------------------------------------------------------------------------

ATTR_COLD const pstring_vector_t base_factory_t::term_param_list()
{
	if (m_def_param.startsWith("+"))
		return pstring_vector_t(m_def_param.substr(1), ",");
	else
		return pstring_vector_t();
}

ATTR_COLD const pstring_vector_t base_factory_t::def_params()
{
	if (m_def_param.startsWith("+") || m_def_param.equals("-"))
		return pstring_vector_t();
	else
		return pstring_vector_t(m_def_param, ",");
}


factory_list_t::factory_list_t( setup_t &setup)
: m_setup(setup)
{
}

factory_list_t::~factory_list_t()
{
	for (std::size_t i=0; i < size(); i++)
	{
		base_factory_t *p = value_at(i);
		pfree(p);
	}
	clear();
}

#if 0
device_t *factory_list_t::new_device_by_classname(const pstring &classname) const
{
	for (std::size_t i=0; i < m_list.size(); i++)
	{
		base_factory_t *p = m_list[i];
		if (p->classname() == classname)
		{
			device_t *ret = p->Create();
			return ret;
		}
		p++;
	}
	return nullptr; // appease code analysis
}
#endif

void factory_list_t::error(const pstring &s)
{
	m_setup.log().fatal("{1}", s);
}

device_t *factory_list_t::new_device_by_name(const pstring &devname, netlist_t &anetlist, const pstring &name)
{
	base_factory_t *f = factory_by_name(devname);
	return f->Create(anetlist, name);
}

base_factory_t * factory_list_t::factory_by_name(const pstring &devname)
{
	if (contains(devname))
		return (*this)[devname];
	else
	{
		m_setup.log().fatal("Class {1} not found!\n", devname);
		return nullptr; // appease code analysis
	}
}

}
