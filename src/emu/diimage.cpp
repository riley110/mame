// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic
/***************************************************************************

    diimage.c

    Device image interfaces.

***************************************************************************/

#include "emu.h"

#include "emuopts.h"
#include "drivenum.h"
#include "ui/uimain.h"
#include "zippath.h"
#include "softlist.h"
#include "formats/ioprocs.h"

#include <regex>


//**************************************************************************
//  DEVICE CONFIG IMAGE INTERFACE
//**************************************************************************
const image_device_type_info device_image_interface::m_device_info_array[] =
	{
		{ IO_UNKNOWN,   "unknown",      "unkn" },
		{ IO_CARTSLOT,  "cartridge",    "cart" }, /*  0 */
		{ IO_FLOPPY,    "floppydisk",   "flop" }, /*  1 */
		{ IO_HARDDISK,  "harddisk",     "hard" }, /*  2 */
		{ IO_CYLINDER,  "cylinder",     "cyln" }, /*  3 */
		{ IO_CASSETTE,  "cassette",     "cass" }, /*  4 */
		{ IO_PUNCHCARD, "punchcard",    "pcrd" }, /*  5 */
		{ IO_PUNCHTAPE, "punchtape",    "ptap" }, /*  6 */
		{ IO_PRINTER,   "printer",      "prin" }, /*  7 */
		{ IO_SERIAL,    "serial",       "serl" }, /*  8 */
		{ IO_PARALLEL,  "parallel",     "parl" }, /*  9 */
		{ IO_SNAPSHOT,  "snapshot",     "dump" }, /* 10 */
		{ IO_QUICKLOAD, "quickload",    "quik" }, /* 11 */
		{ IO_MEMCARD,   "memcard",      "memc" }, /* 12 */
		{ IO_CDROM,     "cdrom",        "cdrm" }, /* 13 */
		{ IO_MAGTAPE,   "magtape",      "magt" }, /* 14 */
		{ IO_ROM,       "romimage",     "rom"  }, /* 15 */
		{ IO_MIDIIN,    "midiin",       "min"  }, /* 16 */
		{ IO_MIDIOUT,   "midiout",      "mout" }  /* 17 */
	};


//**************************************************************************
//  IMAGE DEVICE FORMAT
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

image_device_format::image_device_format(const std::string &name, const std::string &description, const std::string &extensions, const std::string &optspec)
	: m_name(name), m_description(description), m_optspec(optspec)
{
	std::regex comma_regex("\\,");
	std::copy(
		std::sregex_token_iterator(extensions.begin(), extensions.end(), comma_regex, -1),
		std::sregex_token_iterator(),
		std::back_inserter(m_extensions));
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

image_device_format::~image_device_format()
{
}


//**************************************************************************
//  DEVICE IMAGE INTERFACE
//**************************************************************************

//-------------------------------------------------
//  device_image_interface - constructor
//-------------------------------------------------

device_image_interface::device_image_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "image"),
		m_err(),
		m_file(),
		m_mame_file(),
		m_software_info_ptr(nullptr),
		m_software_part_ptr(nullptr),
		m_supported(0),
		m_readonly(false),
		m_created(false),
		m_init_phase(false),
		m_create_format(0),
		m_create_args(nullptr),
		m_user_loadable(TRUE),
		m_is_loading(FALSE)
{
}


//-------------------------------------------------
//  ~device_image_interface - destructor
//-------------------------------------------------

device_image_interface::~device_image_interface()
{
}

//-------------------------------------------------
//  find_device_type - search trough list of
//  device types to extract data
//-------------------------------------------------

const image_device_type_info *device_image_interface::find_device_type(iodevice_t type)
{
	int i;
	for (i = 0; i < ARRAY_LENGTH(device_image_interface::m_device_info_array); i++)
	{
		if (m_device_info_array[i].m_type == type)
			return &m_device_info_array[i];
	}
	return nullptr;
}

//-------------------------------------------------
//  device_typename - retrieves device type name
//-------------------------------------------------

const char *device_image_interface::device_typename(iodevice_t type)
{
	const image_device_type_info *info = find_device_type(type);
	return (info != nullptr) ? info->m_name : nullptr;
}

//-------------------------------------------------
//  device_brieftypename - retrieves device
//  brief type name
//-------------------------------------------------

const char *device_image_interface::device_brieftypename(iodevice_t type)
{
	const image_device_type_info *info = find_device_type(type);
	return (info != nullptr) ? info->m_shortname : nullptr;
}

//-------------------------------------------------
//  device_typeid - retrieves device type id
//-------------------------------------------------

iodevice_t device_image_interface::device_typeid(const char *name)
{
	int i;
	for (i = 0; i < ARRAY_LENGTH(device_image_interface::m_device_info_array); i++)
	{
		if (!core_stricmp(name, m_device_info_array[i].m_name) || !core_stricmp(name, m_device_info_array[i].m_shortname))
			return m_device_info_array[i].m_type;
	}
	return (iodevice_t)-1;
}

/*-------------------------------------------------
    device_compute_hash - compute a hash,
    using this device's partial hash if appropriate
-------------------------------------------------*/

void device_image_interface::device_compute_hash(util::hash_collection &hashes, const void *data, size_t length, const char *types) const
{
	/* retrieve the partial hash func */
	device_image_partialhash_func partialhash = get_partial_hash();

	/* compute the hash */
	if (partialhash)
		partialhash(hashes, (const unsigned char*)data, length, types);
	else
		hashes.compute(reinterpret_cast<const UINT8 *>(data), length, types);
}

//-------------------------------------------------
//  set_image_filename - specifies the filename of
//  an image
//-------------------------------------------------

image_error_t device_image_interface::set_image_filename(const char *filename)
{
	m_image_name = filename;
	util::zippath_parent(m_working_directory, filename);
	m_basename.assign(m_image_name);

	int loc1 = m_image_name.find_last_of('\\');
	int loc2 = m_image_name.find_last_of('/');
	int loc3 = m_image_name.find_last_of(':');
	int loc = MAX(loc1, MAX(loc2, loc3));
	if (loc != -1) {
		if (loc == loc3)
		{
			// temp workaround for softlists now that m_image_name contains the part name too (e.g. list:gamename:cart)
			m_basename = m_basename.substr(0, loc);
			int tmploc = m_basename.find_last_of(':');
			m_basename = m_basename.substr(tmploc + 1, loc - tmploc);
		}
		else
			m_basename = m_basename.substr(loc + 1);
	}
	m_basename_noext = m_basename;
	m_filetype = "";
	loc = m_basename_noext.find_last_of('.');
	if (loc != -1) {
		m_basename_noext = m_basename_noext.substr(0, loc);
		m_filetype = m_basename.substr(loc + 1);
	}

	return IMAGE_ERROR_SUCCESS;
}

/****************************************************************************
    CREATION FORMATS
****************************************************************************/

//-------------------------------------------------
//  device_get_named_creatable_format -
//  accesses a specific image format available for
//  image creation by name
//-------------------------------------------------

const image_device_format *device_image_interface::device_get_named_creatable_format(const char *format_name)
{
	for (auto &format : m_formatlist)
		if (strcmp(format->name().c_str(), format_name) == 0)
			return format.get();
	return nullptr;
}


/****************************************************************************
    ERROR HANDLING
****************************************************************************/

//-------------------------------------------------
//  image_clear_error - clear out any specified
//  error
//-------------------------------------------------

void device_image_interface::clear_error()
{
	m_err = IMAGE_ERROR_SUCCESS;
	if (!m_err_message.empty())
	{
		m_err_message.clear();
	}
}



//-------------------------------------------------
//  error - returns the error text for an image
//  error
//-------------------------------------------------

static const char *const messages[] =
{
	"",
	"Internal error",
	"Unsupported operation",
	"Out of memory",
	"File not found",
	"Invalid image",
	"File already open",
	"Unspecified error"
};

const char *device_image_interface::error()
{
	return (!m_err_message.empty()) ? m_err_message.c_str() : messages[m_err];
}



//-------------------------------------------------
//  seterror - specifies an error on an image
//-------------------------------------------------

void device_image_interface::seterror(image_error_t err, const char *message)
{
	clear_error();
	m_err = err;
	if (message != nullptr)
	{
		m_err_message = message;
	}
}



//-------------------------------------------------
//  message - used to display a message while
//  loading
//-------------------------------------------------

void device_image_interface::message(const char *format, ...)
{
	va_list args;
	char buffer[256];

	/* format the message */
	va_start(args, format);
	vsnprintf(buffer, ARRAY_LENGTH(buffer), format, args);
	va_end(args);

	/* display the popup for a standard amount of time */
	device().machine().ui().popup_time(5, "%s: %s",
		basename(),
		buffer);
}


/***************************************************************************
    WORKING DIRECTORIES
***************************************************************************/

//-------------------------------------------------
//  try_change_working_directory - tries to change
//  the working directory, but only if the directory
//  actually exists
//-------------------------------------------------

bool device_image_interface::try_change_working_directory(const char *subdir)
{
	const osd::directory::entry *entry;
	bool success = false;
	bool done = false;

	auto directory = osd::directory::open(m_working_directory.c_str());
	if (directory)
	{
		while (!done && (entry = directory->read()) != nullptr)
		{
			if (!core_stricmp(subdir, entry->name))
			{
				done = true;
				success = entry->type == osd::directory::entry::entry_type::DIR;
			}
		}

		directory.reset();
	}

	// did we successfully identify the directory?
	if (success)
		m_working_directory = util::zippath_combine(m_working_directory.c_str(), subdir);

	return success;
}


//-------------------------------------------------
//  setup_working_directory - sets up the working
//  directory according to a few defaults
//-------------------------------------------------

void device_image_interface::setup_working_directory()
{
	// first set up the working directory to be the starting directory
	osd_get_full_path(m_working_directory, ".");

	// now try browsing down to "software"
	if (try_change_working_directory("software"))
	{
		// now down to a directory for this computer
		int gamedrv = driver_list::find(device().machine().system());
		while(gamedrv != -1 && !try_change_working_directory(driver_list::driver(gamedrv).name))
		{
			gamedrv = driver_list::compatible_with(gamedrv);
		}
	}
}


//-------------------------------------------------
//  working_directory - returns the working
//  directory to use for this image; this is
//  valid even if not mounted
//-------------------------------------------------

const char * device_image_interface::working_directory()
{
	// check to see if we've never initialized the working directory
	if (m_working_directory.empty())
		setup_working_directory();

	return m_working_directory.c_str();
}


//-------------------------------------------------
//  get_software_region
//-------------------------------------------------

UINT8 *device_image_interface::get_software_region(const char *tag)
{
	char full_tag[256];

	if ( m_software_info_ptr == nullptr || m_software_part_ptr == nullptr )
		return nullptr;

	sprintf( full_tag, "%s:%s", device().tag(), tag );
	memory_region *region = device().machine().root_device().memregion(full_tag);
	return region != nullptr ? region->base() : nullptr;
}


//-------------------------------------------------
//  image_get_software_region_length
//-------------------------------------------------

UINT32 device_image_interface::get_software_region_length(const char *tag)
{
	char full_tag[256];

	sprintf( full_tag, "%s:%s", device().tag(), tag );

	memory_region *region = device().machine().root_device().memregion(full_tag);
	return region != nullptr ? region->bytes() : 0;
}


//-------------------------------------------------
//  image_get_feature
//-------------------------------------------------

const char *device_image_interface::get_feature(const char *feature_name)
{
	return (m_software_part_ptr == nullptr) ? nullptr : m_software_part_ptr->feature(feature_name);
}


//-------------------------------------------------
//  load_software_region -
//-------------------------------------------------

bool device_image_interface::load_software_region(const char *tag, optional_shared_ptr<UINT8> &ptr)
{
	size_t size = get_software_region_length(tag);

	if (size)
	{
		ptr.allocate(size);

		memcpy(ptr, get_software_region(tag), size);
	}

	return size > 0;
}


// ****************************************************************************
// Hash info loading
//
// If the hash is not checked and the relevant info not loaded, force that info
// to be loaded
// ****************************************************************************

void device_image_interface::run_hash(void (*partialhash)(util::hash_collection &, const unsigned char *, unsigned long, const char *),
	util::hash_collection &hashes, const char *types)
{
	UINT32 size;
	dynamic_buffer buf;

	hashes.reset();
	size = (UINT32) length();

	buf.resize(size);
	memset(&buf[0], 0, size);

	// read the file
	fseek(0, SEEK_SET);
	fread(&buf[0], size);

	if (partialhash)
		partialhash(hashes, &buf[0], size, types);
	else
		hashes.compute(&buf[0], size, types);

	// cleanup
	fseek(0, SEEK_SET);
}



void device_image_interface::image_checkhash()
{
	device_image_partialhash_func partialhash;

	// only calculate CRC if it hasn't been calculated, and the open_mode is read only
	UINT32 crcval;
	if (!m_hash.crc(crcval) && m_readonly && !m_created)
	{
		// do not cause a linear read of 600 megs please
		// TODO: use SHA1 in the CHD header as the hash
		if (image_type() == IO_CDROM)
			return;

		// Skip calculating the hash when we have an image mounted through a software list
		if ( m_software_info_ptr )
			return;

		// retrieve the partial hash func
		partialhash = get_partial_hash();

		run_hash(partialhash, m_hash, util::hash_collection::HASH_TYPES_ALL);
	}
	return;
}

UINT32 device_image_interface::crc()
{
	UINT32 crc = 0;

	image_checkhash();
	m_hash.crc(crc);

	return crc;
}

// ****************************************************************************
// Battery functions
//
// These functions provide transparent access to battery-backed RAM on an
// image; typically for cartridges.
// ****************************************************************************


//-------------------------------------------------
//  battery_load - retrieves the battery
//  backed RAM for an image. The file name is
//  created from the machine driver name and the
//  image name.
//-------------------------------------------------

void device_image_interface::battery_load(void *buffer, int length, int fill)
{
	assert_always(buffer && (length > 0), "Must specify sensical buffer/length");

	osd_file::error filerr;
	int bytes_read = 0;
	std::string fname = std::string(device().machine().system().name).append(PATH_SEPARATOR).append(m_basename_noext.c_str()).append(".nv");

	/* try to open the battery file and read it in, if possible */
	emu_file file(device().machine().options().nvram_directory(), OPEN_FLAG_READ);
	filerr = file.open(fname.c_str());
	if (filerr == osd_file::error::NONE)
		bytes_read = file.read(buffer, length);

	// fill remaining bytes (if necessary)
	memset(((char *)buffer) + bytes_read, fill, length - bytes_read);
}

void device_image_interface::battery_load(void *buffer, int length, void *def_buffer)
{
	assert_always(buffer && (length > 0), "Must specify sensical buffer/length");

	osd_file::error filerr;
	int bytes_read = 0;
	std::string fname = std::string(device().machine().system().name).append(PATH_SEPARATOR).append(m_basename_noext.c_str()).append(".nv");

	// try to open the battery file and read it in, if possible
	emu_file file(device().machine().options().nvram_directory(), OPEN_FLAG_READ);
	filerr = file.open(fname.c_str());
	if (filerr == osd_file::error::NONE)
		bytes_read = file.read(buffer, length);

	// if no file was present, copy the default battery
	if (bytes_read == 0 && def_buffer)
		memcpy((char *)buffer, (char *)def_buffer, length);
}


//-------------------------------------------------
//  battery_save - stores the battery
//  backed RAM for an image. The file name is
//  created from the machine driver name and the
//  image name.
//-------------------------------------------------

void device_image_interface::battery_save(const void *buffer, int length)
{
	assert_always(buffer && (length > 0), "Must specify sensical buffer/length");
	std::string fname = std::string(device().machine().system().name).append(PATH_SEPARATOR).append(m_basename_noext.c_str()).append(".nv");

	// try to open the battery file and write it out, if possible
	emu_file file(device().machine().options().nvram_directory(), OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);
	osd_file::error filerr = file.open(fname.c_str());
	if (filerr == osd_file::error::NONE)
		file.write(buffer, length);
}


//-------------------------------------------------
//  uses_file_extension - update configuration
//  based on completed device setup
//-------------------------------------------------

bool device_image_interface::uses_file_extension(const char *file_extension) const
{
	bool result = FALSE;

	if (file_extension[0] == '.')
		file_extension++;

	/* find the extensions */
	std::string extensions(file_extensions());
	char *ext = strtok((char*)extensions.c_str(),",");
	while (ext != nullptr)
	{
		if (!core_stricmp(ext, file_extension))
		{
			result = TRUE;
			break;
		}
		ext = strtok (nullptr, ",");
	}
	return result;
}


// ***************************************************************************
// IMAGE LOADING
// ***************************************************************************

//-------------------------------------------------
//  is_loaded - quick check to determine whether an
//  image is loaded
//-------------------------------------------------

bool device_image_interface::is_loaded()
{
	return (m_file != nullptr);
}


//-------------------------------------------------
//  load_image_by_path - loads an image with a
//  specific path
//-------------------------------------------------

image_error_t device_image_interface::load_image_by_path(UINT32 open_flags, const char *path)
{
	image_error_t err;
	std::string revised_path;

	// attempt to read the file
	auto const filerr = util::zippath_fopen(path, open_flags, m_file, revised_path);

	// did the open succeed?
	switch(filerr)
	{
		case osd_file::error::NONE:
			// success!
			m_readonly = (open_flags & OPEN_FLAG_WRITE) ? 0 : 1;
			m_created = (open_flags & OPEN_FLAG_CREATE) ? 1 : 0;
			err = IMAGE_ERROR_SUCCESS;
			break;

		case osd_file::error::NOT_FOUND:
		case osd_file::error::ACCESS_DENIED:
			// file not found (or otherwise cannot open); continue
			err = IMAGE_ERROR_FILENOTFOUND;
			break;

		case osd_file::error::OUT_OF_MEMORY:
			// out of memory
			err = IMAGE_ERROR_OUTOFMEMORY;
			break;

		case osd_file::error::ALREADY_OPEN:
			// this shouldn't happen
			err = IMAGE_ERROR_ALREADYOPEN;
			break;

		case osd_file::error::FAILURE:
		case osd_file::error::TOO_MANY_FILES:
		case osd_file::error::INVALID_DATA:
		default:
			// other errors
			err = IMAGE_ERROR_INTERNAL;
			break;
	}

	// if successful, set the file name
	if (filerr == osd_file::error::NONE)
		set_image_filename(revised_path.c_str());

	return err;
}

int device_image_interface::reopen_for_write(const char *path)
{
	m_file.reset();

	image_error_t err;
	std::string revised_path;

	// attempt to open the file for writing
	auto const filerr = util::zippath_fopen(path, OPEN_FLAG_READ|OPEN_FLAG_WRITE|OPEN_FLAG_CREATE, m_file, revised_path);

	// did the open succeed?
	switch(filerr)
	{
		case osd_file::error::NONE:
			// success!
			m_readonly = 0;
			m_created = 1;
			err = IMAGE_ERROR_SUCCESS;
			break;

		case osd_file::error::NOT_FOUND:
		case osd_file::error::ACCESS_DENIED:
			// file not found (or otherwise cannot open); continue
			err = IMAGE_ERROR_FILENOTFOUND;
			break;

		case osd_file::error::OUT_OF_MEMORY:
			// out of memory
			err = IMAGE_ERROR_OUTOFMEMORY;
			break;

		case osd_file::error::ALREADY_OPEN:
			// this shouldn't happen
			err = IMAGE_ERROR_ALREADYOPEN;
			break;

		case osd_file::error::FAILURE:
		case osd_file::error::TOO_MANY_FILES:
		case osd_file::error::INVALID_DATA:
		default:
			// other errors
			err = IMAGE_ERROR_INTERNAL;
			break;
	}

	// if successful, set the file name
	if (filerr == osd_file::error::NONE)
		set_image_filename(revised_path.c_str());

	return err;
}


//-------------------------------------------------
//  determine_open_plan - determines which open
//  flags to use, and in what order
//-------------------------------------------------

void device_image_interface::determine_open_plan(int is_create, UINT32 *open_plan)
{
	int i = 0;

	// emit flags
	if (!is_create && is_readable() && is_writeable())
		open_plan[i++] = OPEN_FLAG_READ | OPEN_FLAG_WRITE;
	if (!is_create && !is_readable() && is_writeable())
		open_plan[i++] = OPEN_FLAG_WRITE;
	if (!is_create && is_readable())
		open_plan[i++] = OPEN_FLAG_READ;
	if (is_writeable() && is_creatable())
		open_plan[i++] = OPEN_FLAG_READ | OPEN_FLAG_WRITE | OPEN_FLAG_CREATE;
	open_plan[i] = 0;
}


//-------------------------------------------------
//  dump_wrong_and_correct_checksums - dump an
//  error message containing the wrong and the
//  correct checksums for a given software item
//-------------------------------------------------

static void dump_wrong_and_correct_checksums(const util::hash_collection &hashes, const util::hash_collection &acthashes)
{
	osd_printf_error("    EXPECTED: %s\n", hashes.macro_string().c_str());
	osd_printf_error("       FOUND: %s\n", acthashes.macro_string().c_str());
}


//-------------------------------------------------
//  verify_length_and_hash - verify the length
//  and hash signatures of a file
//-------------------------------------------------

static int verify_length_and_hash(emu_file *file, const char *name, UINT32 explength, const util::hash_collection &hashes)
{
	int retVal = 0;
	if (file==nullptr) return 0;

	// verify length
	UINT32 actlength = file->size();
	if (explength != actlength)
	{
		osd_printf_error("%s WRONG LENGTH (expected: %d found: %d)\n", name, explength, actlength);
		retVal++;
	}

	// If there is no good dump known, write it
	util::hash_collection &acthashes = file->hashes(hashes.hash_types().c_str());
	if (hashes.flag(util::hash_collection::FLAG_NO_DUMP))
	{
		osd_printf_error("%s NO GOOD DUMP KNOWN\n", name);
	}
	// verify checksums
	else if (hashes != acthashes)
	{
		// otherwise, it's just bad
		osd_printf_error("%s WRONG CHECKSUMS:\n", name);
		dump_wrong_and_correct_checksums(hashes, acthashes);
		retVal++;
	}
	// If it matches, but it is actually a bad dump, write it
	else if (hashes.flag(util::hash_collection::FLAG_BAD_DUMP))
	{
		osd_printf_error("%s NEEDS REDUMP\n",name);
	}
	return retVal;
}


//-------------------------------------------------
//  load_software - software image loading
//-------------------------------------------------

bool device_image_interface::load_software(software_list_device &swlist, const char *swname, const rom_entry *start)
{
	std::string locationtag, breakstr("%");
	const rom_entry *region;
	bool retVal = FALSE;
	int warningcount = 0;
	for (region = start; region != nullptr; region = rom_next_region(region))
	{
		// loop until we hit the end of this region
		const rom_entry *romp = region + 1;
		while (!ROMENTRY_ISREGIONEND(romp))
		{
			// handle files
			if (ROMENTRY_ISFILE(romp))
			{
				osd_file::error filerr = osd_file::error::NOT_FOUND;

				UINT32 crc = 0;
				bool has_crc = util::hash_collection(ROM_GETHASHDATA(romp)).crc(crc);

				const software_info *swinfo = swlist.find(swname);
				if (swinfo == nullptr)
					return false;

				UINT32 supported = swinfo->supported();
				if (supported == SOFTWARE_SUPPORTED_PARTIAL)
					osd_printf_error("WARNING: support for software %s (in list %s) is only partial\n", swname, swlist.list_name());
				if (supported == SOFTWARE_SUPPORTED_NO)
					osd_printf_error("WARNING: support for software %s (in list %s) is only preliminary\n", swname, swlist.list_name());

				// attempt reading up the chain through the parents and create a locationtag std::string in the format
				// " swlist % clonename % parentname "
				// below, we have the code to split the elements and to create paths to load from

				while (swinfo != nullptr)
				{
					locationtag.append(swinfo->shortname()).append(breakstr);
					swinfo = !swinfo->parentname().empty() ? swlist.find(swinfo->parentname().c_str()) : nullptr;
				}
				// strip the final '%'
				locationtag.erase(locationtag.length() - 1, 1);


				// check if locationtag actually contains two locations separated by '%'
				// (i.e. check if we are dealing with a clone in softwarelist)
				std::string tag2, tag3, tag4(locationtag), tag5;
				int separator = tag4.find_first_of('%');
				if (separator != -1)
				{
					// we are loading a clone through softlists, split the setname from the parentname
					tag5.assign(tag4.substr(separator + 1, tag4.length() - separator + 1));
					tag4.erase(separator, tag4.length() - separator);
				}

				// prepare locations where we have to load from: list/parentname & list/clonename
				std::string tag1(swlist.list_name());
				tag1.append(PATH_SEPARATOR);
				tag2.assign(tag1.append(tag4));
				tag1.assign(swlist.list_name());
				tag1.append(PATH_SEPARATOR);
				tag3.assign(tag1.append(tag5));

				if (tag5.find_first_of('%') != -1)
					fatalerror("We do not support clones of clones!\n");

				// try to load from the available location(s):
				// - if we are not using lists, we have regiontag only;
				// - if we are using lists, we have: list/clonename, list/parentname, clonename, parentname
				// try to load from list/setname
				if ((m_mame_file == nullptr) && (tag2.c_str() != nullptr))
					m_mame_file = common_process_file(device().machine().options(), tag2.c_str(), has_crc, crc, romp, filerr);
				// try to load from list/parentname
				if ((m_mame_file == nullptr) && (tag3.c_str() != nullptr))
					m_mame_file = common_process_file(device().machine().options(), tag3.c_str(), has_crc, crc, romp, filerr);
				// try to load from setname
				if ((m_mame_file == nullptr) && (tag4.c_str() != nullptr))
					m_mame_file = common_process_file(device().machine().options(), tag4.c_str(), has_crc, crc, romp, filerr);
				// try to load from parentname
				if ((m_mame_file == nullptr) && (tag5.c_str() != nullptr))
					m_mame_file = common_process_file(device().machine().options(), tag5.c_str(), has_crc, crc, romp, filerr);

				warningcount += verify_length_and_hash(m_mame_file.get(),ROM_GETNAME(romp),ROM_GETLENGTH(romp), util::hash_collection(ROM_GETHASHDATA(romp)));

				if (filerr == osd_file::error::NONE)
					filerr = util::core_file::open_proxy(*m_mame_file, m_file);
				if (filerr == osd_file::error::NONE)
					retVal = TRUE;

				break; // load first item for start
			}
			romp++; /* something else; skip */
		}
	}
	if (warningcount > 0)
	{
		osd_printf_error("WARNING: the software item might not run correctly.\n");
	}
	return retVal;
}


//-------------------------------------------------
//  load_internal - core image loading
//-------------------------------------------------

bool device_image_interface::load_internal(const char *path, bool is_create, int create_format, util::option_resolution *create_args, bool just_load)
{
	UINT32 open_plan[4];
	int i;
	bool softload = FALSE;

	// if the path contains no period, we are using softlists, so we won't create an image
	std::string pathstr(path);
	bool filename_has_period = (pathstr.find_last_of('.') != -1) ? TRUE : FALSE;

	// first unload the image
	unload();

	// clear any possible error messages
	clear_error();

	// we are now loading
	m_is_loading = TRUE;

	// record the filename
	m_err = set_image_filename(path);

	if (m_err)
		goto done;

	if (core_opens_image_file())
	{
		// Check if there's a software list defined for this device and use that if we're not creating an image
		if (!filename_has_period && !just_load)
		{
			softload = load_software_part(path, m_software_part_ptr);
			if (softload)
			{
				m_software_info_ptr = &m_software_part_ptr->info();
				m_software_list_name.assign(m_software_info_ptr->list().list_name());
				m_full_software_name.assign(m_software_part_ptr->info().shortname());

				// if we had launched from softlist with a specified part, e.g. "shortname:part"
				// we would have recorded the wrong name, so record it again based on software_info
				if (m_software_info_ptr && !m_full_software_name.empty())
					m_err = set_image_filename(m_full_software_name.c_str());

				// check if image should be read-only
				const char *read_only = get_feature("read_only");
				if (read_only && !strcmp(read_only, "true")) {
					make_readonly();
				}
			}
		}

		if (is_create || filename_has_period)
		{
			// determine open plan
			determine_open_plan(is_create, open_plan);

			// attempt to open the file in various ways
			for (i = 0; !m_file && open_plan[i]; i++)
			{
				// open the file
				m_err = load_image_by_path(open_plan[i], path);
				if (m_err && (m_err != IMAGE_ERROR_FILENOTFOUND))
					goto done;
			}
		}

		// Copy some image information when we have been loaded through a software list
		if ( m_software_info_ptr )
		{
			// sanitize
			if (m_software_info_ptr->longname().empty() || m_software_info_ptr->publisher().empty() || m_software_info_ptr->year().empty())
				fatalerror("Each entry in an XML list must have all of the following fields: description, publisher, year!\n");

			// store
			m_longname = m_software_info_ptr->longname();
			m_manufacturer = m_software_info_ptr->publisher();
			m_year = m_software_info_ptr->year();
			//m_playable = m_software_info_ptr->supported();
		}

		// did we fail to find the file?
		if (!is_loaded() && !softload)
		{
			m_err = IMAGE_ERROR_FILENOTFOUND;
			goto done;
		}
	}

	// call device load or create
	m_create_format = create_format;
	m_create_args = create_args;

	if (m_init_phase==FALSE) {
		m_err = (image_error_t)finish_load();
		if (m_err)
			goto done;
	}
	// success!

done:
	if (just_load) {
		if(m_err) clear();
		return m_err ? IMAGE_INIT_FAIL : IMAGE_INIT_PASS;
	}
	if (m_err!=0) {
		if (!m_init_phase)
		{
			if (device().machine().phase() == MACHINE_PHASE_RUNNING)
				device().popmessage("Error: Unable to %s image '%s': %s", is_create ? "create" : "load", path, error());
			else
				osd_printf_error("Error: Unable to %s image '%s': %s\n", is_create ? "create" : "load", path, error());
		}
		clear();
	}
	else {
		/* do we need to reset the CPU? only schedule it if load/create is successful */
		if (device().machine().time() > attotime::zero && is_reset_on_load())
			device().machine().schedule_hard_reset();
		else
		{
			if (!m_init_phase)
			{
				if (device().machine().phase() == MACHINE_PHASE_RUNNING)
					device().popmessage("Image '%s' was successfully %s.", path, is_create ? "created" : "loaded");
				else
					osd_printf_info("Image '%s' was successfully %s.\n", path, is_create ? "created" : "loaded");
			}
		}
	}
	return m_err ? IMAGE_INIT_FAIL : IMAGE_INIT_PASS;
}



//-------------------------------------------------
//  load - load an image into MAME
//-------------------------------------------------

bool device_image_interface::load(const char *path)
{
	return load_internal(path, FALSE, 0, nullptr, FALSE);
}


//-------------------------------------------------
//  open_image_file - opening plain image file
//-------------------------------------------------

bool device_image_interface::open_image_file(emu_options &options)
{
	const char* path = options.value(instance_name());
	if (*path != 0)
	{
		set_init_phase();
		if (load_internal(path, FALSE, 0, nullptr, TRUE)==IMAGE_INIT_PASS)
		{
			if (software_entry()==nullptr) return true;
		}
	}
	return false;
}


//-------------------------------------------------
//  image_finish_load - special call - only use
//  from core
//-------------------------------------------------

bool device_image_interface::finish_load()
{
	bool err = IMAGE_INIT_PASS;

	if (m_is_loading)
	{
		image_checkhash();

		if (has_been_created())
		{
			err = call_create(m_create_format, m_create_args);
			if (err)
			{
				if (!m_err)
					m_err = IMAGE_ERROR_UNSPECIFIED;
			}
		}
		else
		{
			// using device load
			err = call_load();
			if (err)
			{
				if (!m_err)
					m_err = IMAGE_ERROR_UNSPECIFIED;
			}
		}
	}
	m_is_loading = FALSE;
	m_create_format = 0;
	m_create_args = nullptr;
	m_init_phase = FALSE;
	return err;
}


//-------------------------------------------------
//  create - create a image
//-------------------------------------------------

bool device_image_interface::create(const char *path, const image_device_format *create_format, util::option_resolution *create_args)
{
	int format_index = 0;
	int cnt = 0;
	for (auto &format : m_formatlist)
	{
		if (create_format == format.get()) {
			format_index = cnt;
			break;
		}
		cnt++;
	}
	return load_internal(path, TRUE, format_index, create_args, FALSE);
}


//-------------------------------------------------
//  clear - clear all internal data pertaining
//  to an image
//-------------------------------------------------

void device_image_interface::clear()
{
	m_mame_file.reset();
	m_file.reset();

	m_image_name.clear();
	m_readonly = false;
	m_created = false;

	m_longname.clear();
	m_manufacturer.clear();
	m_year.clear();
	m_basename.clear();
	m_basename_noext.clear();
	m_filetype.clear();

	m_full_software_name.clear();
	m_software_info_ptr = nullptr;
	m_software_part_ptr = nullptr;
	m_software_list_name.clear();
}


//-------------------------------------------------
//  unload - main call to unload an image
//-------------------------------------------------

void device_image_interface::unload()
{
	if (is_loaded() || m_software_info_ptr)
	{
		call_unload();
	}
	clear();
	clear_error();
}


//-------------------------------------------------
//  update_names - update brief and instance names
//-------------------------------------------------

void device_image_interface::update_names(const device_type device_type, const char *inst, const char *brief)
{
	int count = 0;
	int index = -1;
	for (const device_image_interface &image : image_interface_iterator(device().mconfig().root_device()))
	{
		if (this == &image)
			index = count;
		if ((image.image_type() == image_type() && device_type == nullptr) || (device_type == image.device().type()))
			count++;
	}
	const char *inst_name = (device_type!=nullptr) ? inst : device_typename(image_type());
	const char *brief_name = (device_type!=nullptr) ? brief : device_brieftypename(image_type());
	if (count > 1)
	{
		m_instance_name = string_format("%s%d", inst_name, index + 1);
		m_brief_instance_name = string_format("%s%d", brief_name, index + 1);
	}
	else
	{
		m_instance_name = inst_name;
		m_brief_instance_name = brief_name;
	}
}

//-------------------------------------------------
//  software_name_split - helper that splits a
//  software_list:software:part string into
//  separate software_list, software, and part
//  strings.
//
//  str1:str2:str3  => swlist_name - str1, swname - str2, swpart - str3
//  str1:str2       => swlist_name - nullptr, swname - str1, swpart - str2
//  str1            => swlist_name - nullptr, swname - str1, swpart - nullptr
//
//  Notice however that we could also have been
//  passed a string swlist_name:swname, and thus
//  some special check has to be performed in this
//  case.
//-------------------------------------------------

void device_image_interface::software_name_split(const char *swlist_swname, std::string &swlist_name, std::string &swname, std::string &swpart)
{
	// reset all output parameters
	swlist_name.clear();
	swname.clear();
	swpart.clear();

	// if no colon, this is the swname by itself
	const char *split1 = strchr(swlist_swname, ':');
	if (split1 == nullptr)
	{
		swname.assign(swlist_swname);
		return;
	}

	// if one colon, it is the swname and swpart alone
	const char *split2 = strchr(split1 + 1, ':');
	if (split2 == nullptr)
	{
		swname.assign(swlist_swname, split1 - swlist_swname);
		swpart.assign(split1 + 1);
		return;
	}

	// if two colons present, split into 3 parts
	swlist_name.assign(swlist_swname, split1 - swlist_swname);
	swname.assign(split1 + 1, split2 - (split1 + 1));
	swpart.assign(split2 + 1);
}


const software_part *device_image_interface::find_software_item(const char *path, bool restrict_to_interface) const
{
	// split full software name into software list name and short software name
	std::string swlist_name, swinfo_name, swpart_name;
	software_name_split(path, swlist_name, swinfo_name, swpart_name);

	// determine interface
	const char *interface = nullptr;
	if (restrict_to_interface)
		interface = image_interface();

	// find the software list if explicitly specified
	for (software_list_device &swlistdev : software_list_device_iterator(device().mconfig().root_device()))
	{
		if (swlist_name.compare(swlistdev.list_name())==0 || !(swlist_name.length() > 0))
		{
			const software_info *info = swlistdev.find(swinfo_name.c_str());
			if (info != nullptr)
			{
				const software_part *part = info->find_part(swpart_name.c_str(), interface);
				if (part != nullptr)
					return part;
			}
		}

		if (swinfo_name == swlistdev.list_name())
		{
			// ad hoc handling for the case path = swlist_name:swinfo_name (e.g.
			// gameboy:sml) which is not handled properly by software_name_split
			// since the function cannot distinguish between this and the case
			// path = swinfo_name:swpart_name
			const software_info *info = swlistdev.find(swpart_name.c_str());
			if (info != nullptr)
			{
				const software_part *part = info->find_part(nullptr, interface);
				if (part != nullptr)
					return part;
			}
		}
	}

	// if explicitly specified and not found, just error here
	return nullptr;
}


//-------------------------------------------------
//  load_software_part
//
//  Load a software part for a device. The part to
//  load is determined by the "path", software lists
//  configured for a driver, and the interface
//  supported by the device.
//
//  returns true if the software could be loaded,
//  false otherwise. If the software could be loaded
//  sw_info and sw_part are also set.
//-------------------------------------------------

bool device_image_interface::load_software_part(const char *path, const software_part *&swpart)
{
	// if no match has been found, we suggest similar shortnames
	swpart = find_software_item(path, true);
	if (swpart == nullptr)
	{
		software_list_device::display_matches(device().machine().config(), image_interface(), path);
		return false;
	}

	// Load the software part
	software_list_device &swlist = swpart->info().list();
	const char *swname = swpart->info().shortname().c_str();
	const rom_entry *start_entry = swpart->romdata();
	const software_list_loader &loader = get_software_list_loader();
	bool result = loader.load_software(*this, swlist, swname, start_entry);

#ifdef UNUSED_VARIABLE
	// Tell the world which part we actually loaded
	std::string full_sw_name = string_format("%s:%s:%s", swlist.list_name(), swpart->info().shortname(), swpart->name());
#endif

	// check compatibility
	switch (swpart->is_compatible(swlist))
	{
		case SOFTWARE_IS_COMPATIBLE:
			break;

		case SOFTWARE_IS_INCOMPATIBLE:
			swlist.popmessage("WARNING! the set %s might not work on this system due to incompatible filter(s) '%s'\n", swpart->info().shortname(), swlist.filter());
			break;

		case SOFTWARE_NOT_COMPATIBLE:
			swlist.popmessage("WARNING! the set %s might not work on this system due to missing filter(s) '%s'\n", swpart->info().shortname(), swlist.filter());
			break;
	}

	// check requirements and load those images
	const char *requirement = swpart->feature("requirement");
	if (requirement != nullptr)
	{
		const software_part *req_swpart = find_software_item(requirement, false);
		if (req_swpart != nullptr)
		{
			device_image_interface *req_image = req_swpart->find_mountable_image(device().mconfig());
			if (req_image != nullptr)
			{
				req_image->set_init_phase();
				req_image->load(requirement);
			}
		}
	}
	return result;
}

//-------------------------------------------------
//  software_get_default_slot
//-------------------------------------------------

std::string device_image_interface::software_get_default_slot(const char *default_card_slot) const
{
	const char *path = device().mconfig().options().value(instance_name());
	std::string result;
	if (*path != '\0')
	{
		result.assign(default_card_slot);
		const software_part *swpart = find_software_item(path, true);
		if (swpart != nullptr)
		{
			const char *slot = swpart->feature("slot");
			if (slot != nullptr)
				result.assign(slot);
		}
	}
	return result;
}

//----------------------------------------------------------------------------

static int image_fseek_thunk(void *file, INT64 offset, int whence)
{
	device_image_interface *image = (device_image_interface *) file;
	return image->fseek(offset, whence);
}

static size_t image_fread_thunk(void *file, void *buffer, size_t length)
{
	device_image_interface *image = (device_image_interface *) file;
	return image->fread(buffer, length);
}

static size_t image_fwrite_thunk(void *file, const void *buffer, size_t length)
{
	device_image_interface *image = (device_image_interface *) file;
	return image->fwrite(buffer, length);
}

static UINT64 image_fsize_thunk(void *file)
{
	device_image_interface *image = (device_image_interface *) file;
	return image->length();
}

//----------------------------------------------------------------------------

struct io_procs image_ioprocs =
{
	nullptr,
	image_fseek_thunk,
	image_fread_thunk,
	image_fwrite_thunk,
	image_fsize_thunk
};
