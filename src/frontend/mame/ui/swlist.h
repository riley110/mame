// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria, Aaron Giles, Nathan Woods
/***************************************************************************

    ui/swlist.h

    Internal MAME user interface for software list.

***************************************************************************/

#ifndef MAME_FRONTEND_UI_SWLIST_H
#define MAME_FRONTEND_UI_SWLIST_H

#include "ui/menu.h"


namespace ui {
// ======================> menu_software_parts

class menu_software_parts : public menu
{
public:
	enum { T_EMPTY, T_FMGR, T_SWLIST, T_ENTRY };
	menu_software_parts(mame_ui_manager &mui, render_container *container, const software_info *info, const char *interface, const software_part **part, bool other_opt, int *result);
	virtual ~menu_software_parts() override;
	virtual void populate() override;
	virtual void handle() override;

private:
	struct software_part_menu_entry {
		int type;
		const software_part *part;
	};

	// variables
	const software_info *   m_info;
	const char *            m_interface;
	const software_part **  m_selected_part;
	bool                    m_other_opt;
	int *                   m_result;
};


// ======================> menu_software_list

class menu_software_list : public menu
{
public:
	menu_software_list(mame_ui_manager &mui, render_container *container, software_list_device *swlist, const char *interface, std::string &result);
	virtual ~menu_software_list() override;
	virtual void populate() override;
	virtual void handle() override;

private:
	struct entry_info {
		entry_info *next;

		const char *short_name;
		const char *long_name;
	};

	// variables
	software_list_device *          m_swlist; // currently selected list
	const char *                    m_interface;
	std::string &                   m_result;
	entry_info *                    m_entrylist;
	char                            m_filename_buffer[1024];
	bool                            m_ordered_by_shortname;

	// functions
	int compare_entries(const entry_info *e1, const entry_info *e2, bool shortname);
	entry_info *append_software_entry(const software_info &swinfo);
};


// ======================> menu_software

class menu_software : public menu
{
public:
	menu_software(mame_ui_manager &mui, render_container *container, const char *interface, software_list_device **result);
	virtual ~menu_software() override;
	virtual void populate() override;
	virtual void handle() override;

private:
	const char *                    m_interface;
	software_list_device **         m_result;
};

} // namespace ui

#endif  /* MAME_FRONTEND_UI_SWLIST_H */
