// license:BSD-3-Clause
// copyright-holders:Nathan Woods
/***************************************************************************

    ui/imgcntrl.cpp

    MESS's clunky built-in file manager

***************************************************************************/

#include "emu.h"

#include "ui/imgcntrl.h"

#include "ui/ui.h"
#include "ui/filesel.h"
#include "ui/swlist.h"

#include "audit.h"
#include "drivenum.h"
#include "emuopts.h"
#include "softlist.h"
#include "zippath.h"


namespace ui {

/***************************************************************************
    IMPLEMENTATION
***************************************************************************/

//-------------------------------------------------
//  ctor
//-------------------------------------------------

menu_control_device_image::menu_control_device_image(mame_ui_manager &mui, render_container *container, device_image_interface *_image)
	: menu(mui, container),
		submenu_result(0),
		create_ok(false),
		create_confirmed(false)
{
	image = _image;

	if (image->software_list_name())
		sld = software_list_device::find_by_name(mui.machine().config(), image->software_list_name());
	else
		sld = nullptr;
	swi = image->software_entry();
	swp = image->part_entry();

	if(swi)
	{
		state = START_OTHER_PART;
		current_directory.assign(image->working_directory());
	}
	else
	{
		state = START_FILE;

		/* if the image exists, set the working directory to the parent directory */
		if (image->exists())
		{
			current_file.assign(image->filename());
			util::zippath_parent(current_directory, current_file.c_str());
		} else
			current_directory.assign(image->working_directory());

		/* check to see if the path exists; if not clear it */
		if (util::zippath_opendir(current_directory.c_str(), nullptr) != osd_file::error::NONE)
			current_directory.clear();
	}
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

menu_control_device_image::~menu_control_device_image()
{
}


//-------------------------------------------------
//  test_create - creates a new disk image
//-------------------------------------------------

void menu_control_device_image::test_create(bool &can_create, bool &need_confirm)
{
	std::string path;
	osd::directory::entry::entry_type file_type;

	/* assemble the full path */
	util::zippath_combine(path, current_directory.c_str(), current_file.c_str());

	/* does a file or a directory exist at the path */
	auto entry = osd_stat(path.c_str());
	file_type = (entry != nullptr) ? entry->type : osd::directory::entry::entry_type::NONE;

	switch(file_type)
	{
		case osd::directory::entry::entry_type::NONE:
			/* no file/dir here - always create */
			can_create = true;
			need_confirm = false;
			break;

		case osd::directory::entry::entry_type::FILE:
			/* a file exists here - ask for permission from the user */
			can_create = true;
			need_confirm = true;
			break;

		case osd::directory::entry::entry_type::DIR:
			/* a directory exists here - we can't save over it */
			ui().popup_time(5, "%s", _("Cannot save over directory"));
			can_create = false;
			need_confirm = false;
			break;

		default:
			can_create = false;
			need_confirm = false;
			fatalerror("Unexpected\n");
	}
}


//-------------------------------------------------
//  load_software_part
//-------------------------------------------------

void menu_control_device_image::load_software_part()
{
	std::string temp_name = std::string(sld->list_name()).append(":").append(swi->shortname()).append(":").append(swp->name());

	driver_enumerator drivlist(machine().options(), machine().options().system_name());
	drivlist.next();
	media_auditor auditor(drivlist);
	media_auditor::summary summary = auditor.audit_software(sld->list_name(), (software_info *)swi, AUDIT_VALIDATE_FAST);
	// if everything looks good, load software
	if (summary == media_auditor::CORRECT || summary == media_auditor::BEST_AVAILABLE || summary == media_auditor::NONE_NEEDED)
		hook_load(temp_name, true);
	else
	{
		machine().popmessage(_("The software selected is missing one or more required ROM or CHD images. Please select a different one."));
		state = SELECT_SOFTLIST;
	}
}


//-------------------------------------------------
//  hook_load
//-------------------------------------------------

void menu_control_device_image::hook_load(std::string name, bool softlist)
{
	if (image->is_reset_on_load()) image->set_init_phase();
	image->load(name.c_str());
	menu::stack_pop(machine());
}


//-------------------------------------------------
//  populate
//-------------------------------------------------

void menu_control_device_image::populate()
{
}


//-------------------------------------------------
//  handle
//-------------------------------------------------

void menu_control_device_image::handle()
{
	switch(state) {
	case START_FILE: {
		bool can_create = false;
		if(image->is_creatable()) {
			util::zippath_directory *directory = nullptr;
			osd_file::error err = util::zippath_opendir(current_directory.c_str(), &directory);
			can_create = err == osd_file::error::NONE && !util::zippath_is_zip(directory);
			if(directory)
				util::zippath_closedir(directory);
		}
		submenu_result = -1;
		menu::stack_push<menu_file_selector>(ui(), container, image, current_directory, current_file, true, image->image_interface()!=nullptr, can_create, &submenu_result);
		state = SELECT_FILE;
		break;
	}

	case START_SOFTLIST:
		sld = nullptr;
		menu::stack_push<menu_software>(ui(), container, image->image_interface(), &sld);
		state = SELECT_SOFTLIST;
		break;

	case START_OTHER_PART: {
		submenu_result = -1;
		menu::stack_push<menu_software_parts>(ui(), container, swi, swp->interface(), &swp, true, &submenu_result);
		state = SELECT_OTHER_PART;
		break;
	}

	case SELECT_SOFTLIST:
		if(!sld) {
			menu::stack_pop(machine());
			break;
		}
		software_info_name = "";
		menu::stack_push<menu_software_list>(ui(), container, sld, image->image_interface(), software_info_name);
		state = SELECT_PARTLIST;
		break;

	case SELECT_PARTLIST:
		swi = sld->find(software_info_name.c_str());
		if (!swi)
			state = START_SOFTLIST;
		else if(swi->has_multiple_parts(image->image_interface()))
		{
			submenu_result = -1;
			swp = nullptr;
			menu::stack_push<menu_software_parts>(ui(), container, swi, image->image_interface(), &swp, false, &submenu_result);
			state = SELECT_ONE_PART;
		}
		else
		{
			swp = swi->first_part();
			load_software_part();
		}
		break;

	case SELECT_ONE_PART:
		switch(submenu_result) {
		case menu_software_parts::T_ENTRY: {
			load_software_part();
			break;
		}

		case -1: // return to list
			state = SELECT_SOFTLIST;
			break;

		}
		break;

	case SELECT_OTHER_PART:
		switch(submenu_result) {
		case menu_software_parts::T_ENTRY:
			load_software_part();
			break;

		case menu_software_parts::T_FMGR:
			state = START_FILE;
			handle();
			break;

		case menu_software_parts::T_EMPTY:
			image->unload();
			menu::stack_pop(machine());
			break;

		case menu_software_parts::T_SWLIST:
			state = START_SOFTLIST;
			handle();
			break;

		case -1: // return to system
			menu::stack_pop(machine());
			break;

		}
		break;

	case SELECT_FILE:
		switch(submenu_result) {
		case menu_file_selector::R_EMPTY:
			image->unload();
			menu::stack_pop(machine());
			break;

		case menu_file_selector::R_FILE:
			hook_load(current_file, false);
			break;

		case menu_file_selector::R_CREATE:
			menu::stack_push<menu_file_create>(ui(), container, image, current_directory, current_file, &create_ok);
			state = CHECK_CREATE;
			break;

		case menu_file_selector::R_SOFTLIST:
			state = START_SOFTLIST;
			handle();
			break;

		case -1: // return to system
			menu::stack_pop(machine());
			break;
		}
		break;

	case CREATE_FILE: {
		bool can_create, need_confirm;
		test_create(can_create, need_confirm);
		if(can_create) {
			if(need_confirm) {
				menu::stack_push<menu_confirm_save_as>(ui(), container, &create_confirmed);
				state = CREATE_CONFIRM;
			} else {
				state = DO_CREATE;
				handle();
			}
		} else {
			state = START_FILE;
			handle();
		}
		break;
	}

	case CREATE_CONFIRM:
		state = create_confirmed ? DO_CREATE : START_FILE;
		handle();
		break;

	case CHECK_CREATE:
		state = create_ok ? CREATE_FILE : START_FILE;
		handle();
		break;

	case DO_CREATE: {
		std::string path;
		util::zippath_combine(path, current_directory.c_str(), current_file.c_str());
		int err = image->create(path.c_str(), nullptr, nullptr);
		if (err != 0)
			machine().popmessage("Error: %s", image->error());
		menu::stack_pop(machine());
		break;
	}
	}
}

} // namespace ui
