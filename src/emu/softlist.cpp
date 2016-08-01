// license:BSD-3-Clause
// copyright-holders:Wilbert Pol
/***************************************************************************

    softlist.c

    Software list construction helpers.

***************************************************************************/

#include "emu.h"
#include "emuopts.h"
#include "softlist.h"
#include "validity.h"
#include "expat.h"

#include <ctype.h>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

typedef std::unordered_map<std::string,const software_info *> softlist_map;


// ======================> softlist_parser

class softlist_parser
{
public:
	// construction (== execution)
	softlist_parser(software_list_device &list, std::ostringstream &errors);

private:
	enum parse_position
	{
		POS_ROOT,
		POS_MAIN,
		POS_SOFT,
		POS_PART,
		POS_DATA
	};

	// internal parsing helpers
	const char *filename() const { return m_list.filename(); }
	const char *infoname() const { return (m_current_info != nullptr) ? m_current_info->shortname().c_str() : "???"; }
	int line() const { return XML_GetCurrentLineNumber(m_parser); }
	int column() const { return XML_GetCurrentColumnNumber(m_parser); }
	const char *parser_error() const { return XML_ErrorString(XML_GetErrorCode(m_parser)); }

	// internal error helpers
	template <typename Format, typename... Params> void parse_error(Format &&fmt, Params &&... args);
	void unknown_tag(const char *tagname) { parse_error("Unknown tag: %s", tagname); }
	void unknown_attribute(const char *attrname) { parse_error("Unknown attribute: %s", attrname); }

	// internal helpers
	template <typename T> std::vector<std::string> parse_attributes(const char **attributes, const T &attrlist);
	bool parse_name_and_value(const char **attributes, std::string &name, std::string &value);
	void add_rom_entry(const char *name, const char *hashdata, UINT32 offset, UINT32 length, UINT32 flags);

	// expat callbacks
	static void start_handler(void *data, const char *tagname, const char **attributes);
	static void data_handler(void *data, const XML_Char *s, int len);
	static void end_handler(void *data, const char *name);

	// internal parsing
	void parse_root_start(const char *tagname, const char **attributes);
	void parse_main_start(const char *tagname, const char **attributes);
	void parse_soft_start(const char *tagname, const char **attributes);
	void parse_part_start(const char *tagname, const char **attributes);
	void parse_data_start(const char *tagname, const char **attributes);
	void parse_soft_end(const char *name);

	// internal parsing state
	software_list_device &  m_list;
	std::ostringstream &    m_errors;
	XML_Parser              m_parser;
	bool                    m_done;
	bool                    m_data_accum_expected;
	std::string             m_data_accum;
	software_info *         m_current_info;
	software_part *         m_current_part;
	parse_position          m_pos;
};



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
const device_type SOFTWARE_LIST = &device_creator<software_list_device>;
false_software_list_loader false_software_list_loader::s_instance;
rom_software_list_loader rom_software_list_loader::s_instance;
image_software_list_loader image_software_list_loader::s_instance;



//**************************************************************************
//  SOFTWARE LIST LOADER
//**************************************************************************

//-------------------------------------------------
//  false_software_list_loader::load_software
//-------------------------------------------------

bool false_software_list_loader::load_software(device_image_interface &device, software_list_device &swlist, const char *swname, const rom_entry *start_entry) const
{
	return false;
}


//-------------------------------------------------
//  rom_software_list_loader::load_software
//-------------------------------------------------

bool rom_software_list_loader::load_software(device_image_interface &device, software_list_device &swlist, const char *swname, const rom_entry *start_entry) const
{
	swlist.machine().rom_load().load_software_part_region(device, swlist, swname, start_entry);
	return true;
}


//-------------------------------------------------
//  image_software_list_loader::load_software
//-------------------------------------------------

bool image_software_list_loader::load_software(device_image_interface &device, software_list_device &swlist, const char *swname, const rom_entry *start_entry) const
{
	return device.load_software(swlist, swname, start_entry);
}


//**************************************************************************
//  FEATURE LIST ITEM
//**************************************************************************

//-------------------------------------------------
//  feature_list_item - constructor
//-------------------------------------------------

feature_list_item::feature_list_item(const std::string &name, const std::string &value)
	: m_next(nullptr),
	m_name(name),
	m_value(value)
{
}


//-------------------------------------------------
//  feature_list_item - constructor
//-------------------------------------------------

feature_list_item::feature_list_item(std::string &&name, std::string &&value)
	: m_next(nullptr),
	m_name(std::move(name)),
	m_value(std::move(value))
{
}


//**************************************************************************
//  SOFTWARE PART
//**************************************************************************

//-------------------------------------------------
//  software_part - constructor
//-------------------------------------------------

software_part::software_part(software_info &info, std::string &&name, std::string &&interface)
	: m_next(nullptr),
		m_info(info),
		m_name(std::move(name)),
		m_interface(std::move(interface))
{
}


//-------------------------------------------------
//  feature - return the value of the given
//  feature, if specified
//-------------------------------------------------

const char *software_part::feature(const std::string &feature_name) const
{
	// scan the feature list for an entry matching feature_name and return the value
	for (const feature_list_item &feature : m_featurelist)
		if (feature.name() == feature_name)
			return feature.value().c_str();
	return nullptr;

}


//-------------------------------------------------
//  is_compatible - determine if we are compatible
//  with the given software_list_device
//-------------------------------------------------

software_compatibility software_part::is_compatible(const software_list_device &swlistdev) const
{
	// get the softlist filter; if null, assume compatible
	const char *filter = swlistdev.filter();
	if (filter == nullptr)
		return SOFTWARE_IS_COMPATIBLE;

	// copy the comma-delimited string and ensure it ends with a final comma
	std::string filt = std::string(filter).append(",");

	// get the incompatibility filter and test against it first if it exists
	const char *incompatibility = feature("incompatibility");
	if (incompatibility != nullptr)
	{
		// copy the comma-delimited string and ensure it ends with a final comma
		std::string incomp = std::string(incompatibility).append(",");

		// iterate over filter items and see if they exist in the list; if so, it's incompatible
		for (int start = 0, end = filt.find_first_of(',',start); end != -1; start = end + 1, end = filt.find_first_of(',', start))
		{
			std::string token(filt, start, end - start + 1);
			if (incomp.find(token) != -1)
				return SOFTWARE_IS_INCOMPATIBLE;
		}
	}

	// get the compatibility feature; if null, assume compatible
	const char *compatibility = feature("compatibility");
	if (compatibility == nullptr)
		return SOFTWARE_IS_COMPATIBLE;

	// copy the comma-delimited string and ensure it ends with a final comma
	std::string comp = std::string(compatibility).append(",");

	// iterate over filter items and see if they exist in the compatibility list; if so, it's compatible
	for (int start = 0, end = filt.find_first_of(',',start); end != -1; start = end + 1, end = filt.find_first_of(',', start))
	{
		std::string token(filt, start, end - start + 1);
		if (comp.find(token) != -1)
			return SOFTWARE_IS_COMPATIBLE;
	}
	return SOFTWARE_NOT_COMPATIBLE;
}


//-------------------------------------------------
//  matches_interface - determine if we match
//  an interface in the provided list
//-------------------------------------------------

bool software_part::matches_interface(const char *interface_list) const
{
	// if we have no interface, then we match by default
	if (m_interface.empty())
		return true;

	// copy the comma-delimited interface list and ensure it ends with a final comma
	std::string interfaces = std::string(interface_list).append(",");

	// then add a comma to the end of our interface and return true if we find it in the list string
	std::string our_interface = std::string(m_interface).append(",");
	return (interfaces.find(our_interface) != -1);
}


//-------------------------------------------------
//  find_mountable_image - find an image interface
//  that can automatically mount this software part
//-------------------------------------------------

device_image_interface *software_part::find_mountable_image(const machine_config &mconfig) const
{
	// if automount="no", don't bother
	const char *mount = feature("automount");
	if (mount != nullptr && strcmp(mount, "no") == 0)
		return nullptr;

	for (device_image_interface &image : image_interface_iterator(mconfig.root_device()))
	{
		const char *interface = image.image_interface();
		if (interface != nullptr && matches_interface(interface))
		{
			// mount only if not already mounted
			const char *option = mconfig.options().value(image.brief_instance_name());
			if (*option == '\0' && !image.filename())

				return &image;
		}
	}
	return nullptr;
}



//**************************************************************************
//  SOFTWARE INFO
//**************************************************************************

//-------------------------------------------------
//  software_info - constructor
//-------------------------------------------------

software_info::software_info(software_list_device &list, std::string &&name, std::string &&parent, const char *supported)
	: m_next(nullptr),
		m_list(list),
		m_supported(SOFTWARE_SUPPORTED_YES),
		m_shortname(std::move(name)),
		m_parentname(std::move(parent))
{
	// handle the supported flag if provided
	if (supported != nullptr)
	{
		if (strcmp(supported, "partial") == 0)
			m_supported = SOFTWARE_SUPPORTED_PARTIAL;
		else if (strcmp(supported, "no") == 0)
			m_supported = SOFTWARE_SUPPORTED_NO;
	}
}


//-------------------------------------------------
//  find_part - find a part by name with an
//  optional interface match
//-------------------------------------------------

const software_part *software_info::find_part(const char *partname, const char *interface) const
{
	// if neither partname nor interface supplied, then we just return the first entry
	if (partname != nullptr && strlen(partname)==0) partname = nullptr;

	if (partname == nullptr && interface == nullptr)
		return &m_partdata.front();

	// look for the part by name and match against the interface if provided
	for (const software_part &part : m_partdata)
		if (partname != nullptr && (partname == part.name()))
		{
			if (interface == nullptr || part.matches_interface(interface))
				return &part;
		}
		else if (partname == nullptr && part.matches_interface(interface))
				return &part;
	return nullptr;
}


//-------------------------------------------------
//  has_multiple_parts - return true if we have
//  more than one part matching the given
//  interface
//-------------------------------------------------

bool software_info::has_multiple_parts(const char *interface) const
{
	int count = 0;

	// increment the count for each match and stop if we hit more than 1
	for (const software_part &part : m_partdata)
		if (part.matches_interface(interface))
			if (++count > 1)
				return true;

	return false;
}



//**************************************************************************
//  SOFTWARE LIST DEVICE
//**************************************************************************

//-------------------------------------------------
//  software_list_device - constructor
//-------------------------------------------------

software_list_device::software_list_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: device_t(mconfig, SOFTWARE_LIST, "Software list", tag, owner, clock, "software_list", __FILE__),
		m_list_type(SOFTWARE_LIST_ORIGINAL_SYSTEM),
		m_filter(nullptr),
		m_parsed(false),
		m_file(mconfig.options().hash_path(), OPEN_FLAG_READ),
		m_description("")
{
}


//-------------------------------------------------
//  static_set_type - configuration helper
//  to set the list type
//-------------------------------------------------

void software_list_device::static_set_type(device_t &device, const char *list, softlist_type list_type)
{
	software_list_device &swlistdev = downcast<software_list_device &>(device);
	swlistdev.m_list_name.assign(list);
	swlistdev.m_list_type = list_type;
}


//-------------------------------------------------
//  static_set_custom_handler - configuration
//  helper to set a custom callback
//-------------------------------------------------

void software_list_device::static_set_filter(device_t &device, const char *filter)
{
	downcast<software_list_device &>(device).m_filter = filter;
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void software_list_device::device_start()
{
}


//-------------------------------------------------
//  find_approx_matches - search ourselves for
//  a list of possible matches of the given name
//  and optional interface
//-------------------------------------------------

void software_list_device::find_approx_matches(const char *name, int matches, const software_info **list, const char *interface)
{
	// if no name, return
	if (name == nullptr || name[0] == 0)
		return;

	// initialize everyone's states
	std::vector<int> penalty(matches);
	for (int matchnum = 0; matchnum < matches; matchnum++)
	{
		penalty[matchnum] = 9999;
		list[matchnum] = nullptr;
	}

	// iterate over our info (will cause a parse if needed)
	for (const software_info &swinfo : get_info())
	{
		const software_part &part = swinfo.parts().front();
		if ((interface == nullptr || part.matches_interface(interface)) && part.is_compatible(*this) == SOFTWARE_IS_COMPATIBLE)
		{
			// pick the best match between driver name and description
			int longpenalty = driver_list::penalty_compare(name, swinfo.longname().c_str());
			int shortpenalty = driver_list::penalty_compare(name, swinfo.shortname().c_str());
			int curpenalty = MIN(longpenalty, shortpenalty);

			// insert into the sorted table of matches
			for (int matchnum = matches - 1; matchnum >= 0; matchnum--)
			{
				// stop if we're worse than the current entry
				if (curpenalty >= penalty[matchnum])
					break;

				// as long as this isn't the last entry, bump this one down
				if (matchnum < matches - 1)
				{
					penalty[matchnum + 1] = penalty[matchnum];
					list[matchnum + 1] = list[matchnum];
				}
				list[matchnum] = &swinfo;
				penalty[matchnum] = curpenalty;
			}
		}
	}
}


//-------------------------------------------------
//  release - reset to a pre-parsed state
//-------------------------------------------------

void software_list_device::release()
{
	osd_printf_verbose("Resetting %s\n", m_file.filename());
	m_parsed = false;
	m_description.clear();
	m_errors.clear();
	m_infolist.clear();
}


//-------------------------------------------------
//  find_by_name - find a software list by name
//  across all software list devices
//-------------------------------------------------

software_list_device *software_list_device::find_by_name(const machine_config &config, const char *name)
{
	// iterate over each device in the system and find a match
	for (software_list_device &swlistdev : software_list_device_iterator(config.root_device()))
		if (strcmp(swlistdev.list_name(), name) == 0)
			return &swlistdev;
	return nullptr;
}


//-------------------------------------------------
//  software_display_matches - display a list of
//  possible matches in the system to the given
//  name, across all software list devices
//-------------------------------------------------

void software_list_device::display_matches(const machine_config &config, const char *interface, const char *name)
{
	// check if there is at least one software list
	software_list_device_iterator deviter(config.root_device());
	if (deviter.first() != nullptr)
		osd_printf_error("\n\"%s\" approximately matches the following\n"
							"supported software items (best match first):\n\n", name);

	// iterate through lists
	for (software_list_device &swlistdev : deviter)
	{
		// get the top 16 approximate matches for the selected device interface (i.e. only carts for cartslot, etc.)
		const software_info *matches[16] = { nullptr };
		swlistdev.find_approx_matches(name, ARRAY_LENGTH(matches), matches, interface);

		// if we found some, print them
		if (matches[0] != nullptr)
		{
			// different output depending on original system or compatible
			if (swlistdev.list_type() == SOFTWARE_LIST_ORIGINAL_SYSTEM)
				osd_printf_error("* Software list \"%s\" (%s) matches: \n", swlistdev.list_name(), swlistdev.description());
			else
				osd_printf_error("* Compatible software list \"%s\" (%s) matches: \n", swlistdev.list_name(), swlistdev.description());

			// print them out
			for (auto &match : matches)
			{
				if (match != nullptr)
					osd_printf_error("%-18s%s\n", match->shortname().c_str(), match->longname().c_str());
			}

			osd_printf_error("\n");
		}
	}
}


//-------------------------------------------------
//  find - find an item by name in the software
//  list, using wildcards and optionally starting
//  from an intermediate point
//-------------------------------------------------

const software_info *software_list_device::find(const char *look_for)
{
	// nullptr search returns nothing
	if (look_for == nullptr)
		return nullptr;

	bool iswild = strchr(look_for, '*') != nullptr || strchr(look_for, '?');

	// find a match (will cause a parse if needed when calling get_info)
	const auto &info_list = get_info();
	auto iter = std::find_if(
		info_list.begin(),
		info_list.end(),
		[&](const software_info &info)
		{
			const char *shortname = info.shortname().c_str();
			return (iswild && core_strwildcmp(look_for, shortname) == 0)
				|| core_stricmp(look_for, shortname) == 0;
		});

	return iter != info_list.end()
		? &*iter
		: nullptr;
}


//-------------------------------------------------
//  parse - parse our softlist file
//-------------------------------------------------

void software_list_device::parse()
{
	// skip if done
	if (m_parsed)
		return;

	// reset the errors
	m_errors.clear();

	// attempt to open the file
	osd_file::error filerr = m_file.open(m_list_name.c_str(), ".xml");
	if (filerr == osd_file::error::NONE)
	{
		// parse if no error
		std::ostringstream errs;
		softlist_parser parser(*this, errs);
		m_file.close();
		m_errors = errs.str();
	}
	else
		m_errors = string_format("Error opening file: %s\n", filename());

	// indicate that we've been parsed
	m_parsed = true;
}


//-------------------------------------------------
//  device_validity_check - validate the device
//  configuration
//-------------------------------------------------

void software_list_device::device_validity_check(validity_checker &valid) const
{
	// add to the global map whenever we check a list so we don't re-check
	// it in the future
	if (valid.already_checked(std::string("softlist/").append(m_list_name).c_str()))
		return;

	// do device validation only in case of validate command
	if (!valid.validate_all())
		return;

	// actually do the validate
	const_cast<software_list_device *>(this)->internal_validity_check(valid);
}


//-------------------------------------------------
//  internal_validity_check - internal helper to
//  check the list
//-------------------------------------------------

void software_list_device::internal_validity_check(validity_checker &valid)
{
	enum { NAME_LEN_PARENT = 8, NAME_LEN_CLONE = 16 };

	softlist_map names;
	softlist_map descriptions;
	for (const software_info &swinfo : get_info())
	{
		// first parse and output core errors if any
		if (m_errors.length() > 0)
		{
			osd_printf_error("%s: Errors parsing software list:\n%s", filename(), errors_string());
			break;
		}

		// Now check if the xml data is valid:

		// Did we lost any description?
		if (swinfo.longname().empty())
		{
			osd_printf_error("%s: %s has no description\n", filename(), swinfo.shortname().c_str());
			break;
		}

		// Did we lost any year?
		if (swinfo.year().empty())
		{
			osd_printf_error("%s: %s has no year\n", filename(), swinfo.shortname().c_str());
			break;
		}

		// Did we lost any publisher?
		if (swinfo.publisher().empty())
		{
			osd_printf_error("%s: %s has no publisher\n", filename(), swinfo.shortname().c_str());
			break;
		}

		// Did we lost the software parts?
		if (swinfo.parts().empty())
		{
			osd_printf_error("%s: %s has no part\n", filename(), swinfo.shortname().c_str());
			break;
		}

		// Second, since the xml is fine, run additional checks:

		// check for duplicate names
		if (!names.insert(std::make_pair(swinfo.shortname(), &swinfo)).second)
		{
			const software_info *match = names.find(swinfo.shortname())->second;
			osd_printf_error("%s: %s is a duplicate name (%s)\n", filename(), swinfo.shortname().c_str(), match->shortname().c_str());
		}

		// check for duplicate descriptions
		std::string longname = std::string(swinfo.longname());
		if (!descriptions.insert(std::make_pair(strmakelower(longname), &swinfo)).second)
			osd_printf_error("%s: %s is a duplicate description (%s)\n", filename(), swinfo.longname().c_str(), swinfo.shortname().c_str());

		bool is_clone = false;
		if (!swinfo.parentname().empty())
		{
			is_clone = true;
			if (swinfo.parentname() == swinfo.shortname())
			{
				osd_printf_error("%s: %s is set as a clone of itself\n", filename(), swinfo.shortname().c_str());
				break;
			}

			// make sure the parent exists
			const software_info *swinfo2 = find(swinfo.parentname().c_str());

			if (swinfo2 == nullptr)
				osd_printf_error("%s: parent '%s' software for '%s' not found\n", filename(), swinfo.parentname().c_str(), swinfo.shortname().c_str());
			else if (!swinfo2->parentname().empty())
				osd_printf_error("%s: %s is a clone of a clone\n", filename(), swinfo.shortname().c_str());
		}

		// make sure the driver name is 8 chars or less
		if ((is_clone && swinfo.shortname().length() > NAME_LEN_CLONE) || (!is_clone && swinfo.shortname().length() > NAME_LEN_PARENT))
			osd_printf_error("%s: %s %s driver name must be %d characters or less\n", filename(), swinfo.shortname().c_str(),
								is_clone ? "clone" : "parent", is_clone ? NAME_LEN_CLONE : NAME_LEN_PARENT);

		// make sure the year is only digits, '?' or '+'
		for (const char *s = swinfo.year().c_str(); *s != 0; s++)
			if (!isdigit((UINT8)*s) && *s != '?' && *s != '+')
			{
				osd_printf_error("%s: %s has an invalid year '%s'\n", filename(), swinfo.shortname().c_str(), swinfo.year().c_str());
				break;
			}

		softlist_map part_names;
		for (const software_part &part : swinfo.parts())
		{
			if (part.interface().empty())
				osd_printf_error("%s: %s has a part (%s) without interface\n", filename(), swinfo.shortname().c_str(), part.name().c_str());

			if (part.romdata() == nullptr)
				osd_printf_error("%s: %s has a part (%s) with no data\n", filename(), swinfo.shortname().c_str(), part.name().c_str());

			if (!part_names.insert(std::make_pair(part.name(), &swinfo)).second)
				osd_printf_error("%s: %s has a part (%s) whose name is duplicate\n", filename(), swinfo.shortname().c_str(), part.name().c_str());

			for (const rom_entry *data = part.romdata(); data->_name != nullptr; data++)
				if (data->_hashdata != nullptr)
				{
					// make sure the hash is valid
					util::hash_collection hashes;
					if (!hashes.from_internal_string(data->_hashdata))
						osd_printf_error("%s: %s has rom '%s' with an invalid hash string '%s'\n", filename(), swinfo.shortname().c_str(), data->_name, data->_hashdata);
				}
		}
	}

	// release all the memory
	release();
}



//**************************************************************************
//  SOFTWARE LIST PARSER
//**************************************************************************

//-------------------------------------------------
//  softlist_parser - constructor
//-------------------------------------------------

softlist_parser::softlist_parser(software_list_device &list, std::ostringstream &errors)
	: m_list(list),
		m_errors(errors),
		m_done(false),
		m_data_accum_expected(false),
		m_current_info(nullptr),
		m_current_part(nullptr),
		m_pos(POS_ROOT)
{
	osd_printf_verbose("Parsing %s\n", m_list.m_file.filename());

	// create the parser
	m_parser = XML_ParserCreate_MM(nullptr, nullptr, nullptr);
	if (m_parser == nullptr)
		throw std::bad_alloc();

	// set the handlers
	XML_SetUserData(m_parser, this);
	XML_SetElementHandler(m_parser, &softlist_parser::start_handler, &softlist_parser::end_handler);
	XML_SetCharacterDataHandler(m_parser, &softlist_parser::data_handler);

	// parse the file contents
	m_list.m_file.seek(0, SEEK_SET);
	char buffer[1024];
	while (!m_done)
	{
		UINT32 length = m_list.m_file.read(buffer, sizeof(buffer));
		m_done = m_list.m_file.eof();
		if (XML_Parse(m_parser, buffer, length, m_done) == XML_STATUS_ERROR)
		{
			parse_error("%s", parser_error());
			break;
		}
	}

	// free the parser
	XML_ParserFree(m_parser);
	osd_printf_verbose("Parsing complete\n");
}

//-------------------------------------------------
//  parse_error - append a parsing error with
//  filename, line and column information
//-------------------------------------------------

template <typename Format, typename... Params>
inline void softlist_parser::parse_error(Format &&fmt, Params &&... args)
{
	// always start with filename(line.column):
	util::stream_format(m_errors, "%s(%d.%d): ", filename(), line(), column());

	// append the remainder of the string
	util::stream_format(m_errors, std::forward<Format>(fmt), std::forward<Params>(args)...);

	// append a newline at the end
	m_errors.put('\n');
}


//-------------------------------------------------
//  parse_attributes - helper to parse a set of
//  attributes into a list of strings
//-------------------------------------------------

template <typename T>
std::vector<std::string> softlist_parser::parse_attributes(const char **attributes, const T &attrlist)
{
	std::vector<std::string> outlist(std::distance(std::begin(attrlist), std::end(attrlist)));

	// iterate over attribute/value pairs
	for( ; attributes[0]; attributes += 2)
	{
		auto iter = std::begin(attrlist);

		// look for a match among the attributes provided
		for (std::size_t index = 0; iter != std::end(attrlist); ++index, ++iter)
		{
			if (strcmp(attributes[0], *iter) == 0)
			{
				// if found, set the corresponding output entry to the value
				outlist[index] = attributes[1];
				break;
			}
		}

		// if not found, report an unknown attribute
		if (iter == std::end(attrlist))
			unknown_attribute(attributes[0]);
	}

	return outlist;
}


//-------------------------------------------------
//  parse_name_and_value - helper to parse "name"
//  and "value" attribute pairs (allowing the
//  latter to be defined as an empty string)
//-------------------------------------------------

bool softlist_parser::parse_name_and_value(const char **attributes, std::string &name, std::string &value)
{
	bool found_value = false;

	// iterate over attribute/value pairs
	for( ; attributes[0]; attributes += 2)
	{
		// if found, set the corresponding output entry to the value
		if (strcmp(attributes[0], "name") == 0)
		{
			name = attributes[1];
		}

		else if (strcmp(attributes[0], "value") == 0)
		{
			value = attributes[1];
			found_value = true;
		}

		// if not found, report an unknown attribute
		else
			unknown_attribute(attributes[0]);
	}

	return !name.empty() && found_value;
}


//-------------------------------------------------
//  add_rom_entry - append a new ROM entry to the
//  current part's list
//-------------------------------------------------

void softlist_parser::add_rom_entry(const char *name, const char *hashdata, UINT32 offset, UINT32 length, UINT32 flags)
{
	// get the current part
	if (m_current_part == nullptr)
	{
		parse_error("ROM entry added in invalid context");
		return;
	}

	// make sure we don't add duplicate regions
	if (name != nullptr && (flags & ROMENTRY_TYPEMASK) == ROMENTRYTYPE_REGION)
		for (auto &elem : m_current_part->m_romdata)
			if (elem._name != nullptr && strcmp(elem._name, name) == 0)
				parse_error("Duplicated dataarea %s in software %s", name, infoname());

	// create the new entry and append it
	rom_entry entry;
	entry._name = m_list.add_string(name);
	entry._hashdata = m_list.add_string(hashdata);
	entry._offset = offset;
	entry._length = length;
	entry._flags = flags;

	m_current_part->m_romdata.push_back(entry);
}


//-------------------------------------------------
//  start_handler - expat handler for tag start
//-------------------------------------------------

void softlist_parser::start_handler(void *data, const char *tagname, const char **attributes)
{
	// switch off the current state
	softlist_parser *state = reinterpret_cast<softlist_parser *>(data);
	switch (state->m_pos)
	{
		case POS_ROOT:
			state->parse_root_start(tagname, attributes);
			break;

		case POS_MAIN:
			state->parse_main_start(tagname, attributes);
			break;

		case POS_SOFT:
			state->parse_soft_start(tagname, attributes);
			break;

		case POS_PART:
			state->parse_part_start(tagname, attributes);
			break;

		case POS_DATA:
			state->parse_data_start(tagname, attributes);
			break;
	}

	// increment the state since this is a tag start
	state->m_pos = parse_position(state->m_pos + 1);
}


//-------------------------------------------------
//  end_handler - handle end-of-tag post-processing
//-------------------------------------------------

void softlist_parser::end_handler(void *data, const char *name)
{
	// reset the text destination and bump the position down
	softlist_parser *state = reinterpret_cast<softlist_parser *>(data);
	state->m_pos = parse_position(state->m_pos - 1);

	// switch off of the new position
	switch (state->m_pos)
	{
		case POS_ROOT:
			break;

		case POS_MAIN:
			state->m_current_info = nullptr;
			break;

		case POS_SOFT:
			state->parse_soft_end(name);
			state->m_current_part = nullptr;
			break;

		case POS_PART:
			break;

		case POS_DATA:
			break;
	}

	// stop accumulating
	state->m_data_accum_expected = false;
	state->m_data_accum.clear();
}


//-------------------------------------------------
//  data_handler - expat data handler
//-------------------------------------------------

void softlist_parser::data_handler(void *data, const XML_Char *s, int len)
{
	softlist_parser *state = reinterpret_cast<softlist_parser *>(data);

	// if we have an std::string to accumulate data in, do it
	if (state->m_data_accum_expected)
		state->m_data_accum.append(s, len);

	// otherwise, report an error if the data is non-blank
	else
		for (int i = 0; i < len; i++)
			if (!isspace(s[i]))
			{
				state->parse_error("Unexpected content");
				break;
			}
}


//-------------------------------------------------
//  parse_root_start - handle tag start at the root
//-------------------------------------------------

void softlist_parser::parse_root_start(const char *tagname, const char **attributes)
{
	// <softwarelist name='' description=''>
	if (strcmp(tagname, "softwarelist") == 0)
	{
		static const char *attrnames[] = { "name", "description" };
		const auto attrvalues = parse_attributes(attributes, attrnames);

		if (!attrvalues[1].empty())
			m_list.m_description = attrvalues[1];
	}
	else
		unknown_tag(tagname);
}


//-------------------------------------------------
//  parse_main_start - handle tag start within
//  a softwarelist tag
//-------------------------------------------------

void softlist_parser::parse_main_start(const char *tagname, const char **attributes)
{
	// <software name='' cloneof='' supported=''>
	if (strcmp(tagname, "software") == 0)
	{
		static const char *attrnames[] = { "name", "cloneof", "supported" };
		auto attrvalues = parse_attributes(attributes, attrnames);

		if (!attrvalues[0].empty())
		{
			m_list.m_infolist.emplace_back(m_list, std::move(attrvalues[0]), std::move(attrvalues[1]), attrvalues[2].c_str());
			m_current_info = &m_list.m_infolist.back();
		}
		else
			parse_error("No name defined for item");
	}
	else
		unknown_tag(tagname);
}


//-------------------------------------------------
//  parse_main_start - handle tag start within
//  a software tag
//-------------------------------------------------

void softlist_parser::parse_soft_start(const char *tagname, const char **attributes)
{
	// get the current info; error if none
	if (m_current_info == nullptr)
	{
		parse_error("Tag %s found outside of software context", tagname);
		return;
	}

	// <description>
	if (strcmp(tagname, "description") == 0)
		m_data_accum_expected = true;

	// <year>
	else if (strcmp(tagname, "year") == 0)
		m_data_accum_expected = true;

	// <publisher>
	else if (strcmp(tagname, "publisher") == 0)
		m_data_accum_expected = true;

	// <info name='' value=''>
	else if (strcmp(tagname, "info") == 0)
	{
		std::string infoname, infovalue;

		if (parse_name_and_value(attributes, infoname, infovalue))
			m_current_info->m_other_info.emplace_back(std::move(infoname), std::move(infovalue));
		else
			parse_error("Incomplete other_info definition");
	}

	// <sharedfeat name='' value=''>
	else if (strcmp(tagname, "sharedfeat") == 0)
	{
		std::string featname, featvalue;

		if (parse_name_and_value(attributes, featname, featvalue))
			m_current_info->m_shared_info.emplace_back(std::move(featname), std::move(featvalue));
		else
			parse_error("Incomplete sharedfeat definition");
	}

	// <part name='' interface=''>
	else if (strcmp(tagname, "part" ) == 0)
	{
		static const char *attrnames[] = { "name", "interface" };
		auto attrvalues = parse_attributes(attributes, attrnames);

		if (!attrvalues[0].empty() && !attrvalues[1].empty())
		{
			m_current_info->m_partdata.emplace_back(*m_current_info, std::move(attrvalues[0]), std::move(attrvalues[1]));
			m_current_part = &m_current_info->m_partdata.back();
		}
		else
			parse_error("Incomplete part definition");
	}
	else
		unknown_tag(tagname);
}


//-------------------------------------------------
//  parse_part_start - handle tag start within
//  a part tag
//-------------------------------------------------

void softlist_parser::parse_part_start(const char *tagname, const char **attributes)
{
	// get the current part; error if none
	if (m_current_part == nullptr)
	{
		parse_error("Tag %s found outside of part context", tagname);
		return;
	}

	// <dataarea name='' size=''>
	if (strcmp(tagname, "dataarea") == 0)
	{
		static const char *attrnames[] = { "name", "size", "width", "endianness" };
		const auto attrvalues = parse_attributes(attributes, attrnames);

		if (!attrvalues[0].empty() && !attrvalues[1].empty())
		{
			// handle region attributes
			const std::string &width = attrvalues[2];
			const std::string &endianness = attrvalues[3];
			UINT32 regionflags = ROMENTRYTYPE_REGION;

			if (!width.empty())
			{
				if (width == "8")
					regionflags |= ROMREGION_8BIT;
				else if (width == "16")
					regionflags |= ROMREGION_16BIT;
				else if (width == "32")
					regionflags |= ROMREGION_32BIT;
				else if (width == "64")
					regionflags |= ROMREGION_64BIT;
				else
					parse_error("Invalid dataarea width");
			}
			if (!endianness.empty())
			{
				if (endianness == "little")
					regionflags |= ROMREGION_LE;
				else if (endianness == "big")
					regionflags |= ROMREGION_BE;
				else
					parse_error("Invalid dataarea endianness");
			}

			add_rom_entry(attrvalues[0].c_str(), nullptr, 0, strtol(attrvalues[1].c_str(), nullptr, 0), regionflags);
		}
		else
			parse_error("Incomplete dataarea definition");
	}

	// <diskarea name=''>
	else if (strcmp(tagname, "diskarea") == 0)
	{
		static const char *attrnames[] = { "name" };
		const auto attrvalues = parse_attributes(attributes, attrnames);

		if (!attrvalues[0].empty())
			add_rom_entry(attrvalues[0].c_str(), nullptr, 0, 1, ROMENTRYTYPE_REGION | ROMREGION_DATATYPEDISK);
		else
			parse_error("Incomplete diskarea definition");
	}

	// <feature name='' value=''>
	else if (strcmp(tagname, "feature") == 0)
	{
		std::string featname, featvalue;

		if (parse_name_and_value(attributes, featname, featvalue))
			m_current_part->m_featurelist.emplace_back(std::move(featname), std::move(featvalue));
		else
			parse_error("Incomplete feature definition");
	}

	// <dipswitch>
	else if (strcmp(tagname, "dipswitch") == 0)
		;
	else
		unknown_tag(tagname);
}


//-------------------------------------------------
//  parse_data_start - handle tag start within a
//  dataarea or diskarea tag
//-------------------------------------------------

void softlist_parser::parse_data_start(const char *tagname, const char **attributes)
{
	// get the current part; error if none
	if (m_current_part == nullptr)
	{
		parse_error("Tag %s found outside of part context", tagname);
		return;
	}

	// <rom name='' size='' crc='' sha1='' offset='' value='' status='' loadflag=''>
	if (strcmp(tagname, "rom") == 0)
	{
		static const char *attrnames[] = { "name", "size", "crc", "sha1", "offset", "value", "status", "loadflag" };
		const auto attrvalues = parse_attributes(attributes, attrnames);

		const std::string &name = attrvalues[0];
		const std::string &sizestr = attrvalues[1];
		const std::string &crc = attrvalues[2];
		const std::string &sha1 = attrvalues[3];
		const std::string &offsetstr = attrvalues[4];
		const std::string &value = attrvalues[5];
		const std::string &status = attrvalues[6];
		const std::string &loadflag = attrvalues[7];
		if (!sizestr.empty() && !offsetstr.empty())
		{
			UINT32 length = strtol(sizestr.c_str(), nullptr, 0);
			UINT32 offset = strtol(offsetstr.c_str(), nullptr, 0);

			if (loadflag == "reload")
				add_rom_entry(nullptr, nullptr, offset, length, ROMENTRYTYPE_RELOAD | ROM_INHERITFLAGS);
			else if (loadflag == "reload_plain")
				add_rom_entry(nullptr, nullptr, offset, length, ROMENTRYTYPE_RELOAD);
			else if (loadflag == "continue")
				add_rom_entry(nullptr, nullptr, offset, length, ROMENTRYTYPE_CONTINUE | ROM_INHERITFLAGS);
			else if (loadflag == "fill")
				add_rom_entry(nullptr, (const char *)(FPTR)(strtol(value.c_str(), nullptr, 0) & 0xff), offset, length, ROMENTRYTYPE_FILL);
			else if (!name.empty())
			{
				bool baddump = (status == "baddump");
				bool nodump = (status == "nodump");

				std::string hashdata;
				if (nodump)
				{
					hashdata = string_format("%s", NO_DUMP);
					if (!crc.empty() && !sha1.empty())
						parse_error("No need for hash definition");
				}
				else
				{
					if (!crc.empty() && !sha1.empty())
						hashdata = string_format("%c%s%c%s%s", util::hash_collection::HASH_CRC, crc, util::hash_collection::HASH_SHA1, sha1, (baddump ? BAD_DUMP : ""));
					else
						parse_error("Incomplete rom hash definition");
				}

				// Handle loadflag attribute
				int romflags = 0;
				if (loadflag == "load16_word_swap")
					romflags = ROM_GROUPWORD | ROM_REVERSE;
				else if (loadflag == "load16_byte")
					romflags = ROM_SKIP(1);
				else if (loadflag == "load32_word_swap")
					romflags = ROM_GROUPWORD | ROM_REVERSE | ROM_SKIP(2);
				else if (loadflag == "load32_word")
					romflags = ROM_GROUPWORD | ROM_SKIP(2);
				else if (loadflag == "load32_byte")
					romflags = ROM_SKIP(3);

				add_rom_entry(name.c_str(), hashdata.c_str(), offset, length, ROMENTRYTYPE_ROM | romflags);
			}
			else
				parse_error("Rom name missing");
		}
		else if (!sizestr.empty() && !loadflag.empty() && loadflag == "ignore")
		{
			UINT32 length = strtol(sizestr.c_str(), nullptr, 0);
			add_rom_entry(nullptr, nullptr, 0, length, ROMENTRYTYPE_IGNORE | ROM_INHERITFLAGS);
		}
		else
			parse_error("Incomplete rom definition");
	}

	// <rom name='' sha1='' status='' writeable=''>
	else if (strcmp(tagname, "disk") == 0)
	{
		static const char *attrnames[] = { "name", "sha1", "status", "writeable" };
		const auto attrvalues = parse_attributes(attributes, attrnames);

		const std::string &name = attrvalues[0];
		const std::string &sha1 = attrvalues[1];
		const std::string &status = attrvalues[2];
		const std::string &writeablestr = attrvalues[3];
		if (!name.empty() && !sha1.empty())
		{
			const bool baddump = (status == "baddump");
			const bool nodump = (status == "nodump" );
			const bool writeable = (writeablestr == "yes");
			std::string hashdata = string_format("%c%s%s", util::hash_collection::HASH_SHA1, sha1, (nodump ? NO_DUMP : (baddump ? BAD_DUMP : "")));

			add_rom_entry(name.c_str(), hashdata.c_str(), 0, 0, ROMENTRYTYPE_ROM | (writeable ? DISK_READWRITE : DISK_READONLY));
		}
		else if (status.empty() || (status == "nodump")) // a no_dump chd is not an incomplete entry
			parse_error("Incomplete disk definition");
	}

	// <dipvalue>
	else if (strcmp(tagname, "dipvalue") == 0)
		;
	else
		unknown_tag(tagname);
}


//-------------------------------------------------
//  parse_soft_end - handle end-of-tag post-
//  processing within the <software> tag
//-------------------------------------------------

void softlist_parser::parse_soft_end(const char *tagname)
{
	assert(m_current_info != nullptr);

	// <description>
	if (strcmp(tagname, "description") == 0)
		m_current_info->m_longname = m_data_accum;

	// <year>
	else if (strcmp(tagname, "year") == 0)
		m_current_info->m_year = m_data_accum;

	// <publisher>
	else if (strcmp(tagname, "publisher") == 0)
		m_current_info->m_publisher = m_data_accum;

	// </part>
	else if (strcmp(tagname, "part") == 0)
	{
		// get the last part
		assert(m_current_part != nullptr);
		if (m_current_part == nullptr)
			return;

		// was any dataarea/rom information encountered? if so, add a terminator
		if (m_current_part->romdata() != nullptr)
			add_rom_entry(nullptr, nullptr, 0, 0, ROMENTRYTYPE_END);

		// get the info; if present, copy shared data (we assume name/value strings live
		// in the string pool and don't need to be reallocated)
		if (m_current_info != nullptr)
			for (const feature_list_item &item : m_current_info->shared_info())
				m_current_part->m_featurelist.emplace_back(item.name(), item.value());
	}
}
