// license:BSD-3-Clause
// copyright-holders:Dankan1890
/***************************************************************************

    ui/optsmenu.h

    UI main options menu manager.

***************************************************************************/

#pragma once

#ifndef __UI_OPTSMENU_H__
#define __UI_OPTSMENU_H__

class ui_menu_game_options : public ui_menu
{
public:
	ui_menu_game_options(running_machine &machine, render_container *container);
	virtual ~ui_menu_game_options();
	virtual void populate() override;
	virtual void handle() override;
	virtual void custom_render(void *selectedref, float top, float bottom, float x, float y, float x2, float y2) override;

private:
	enum
	{
		FILTER_MENU = 1,
		FILE_CATEGORY_FILTER,
		MANUFACT_CAT_FILTER,
		YEAR_CAT_FILTER,
		SCREEN_CAT_FILTER,
		CATEGORY_FILTER,
		MISC_MENU,
		DISPLAY_MENU,
		CUSTOM_MENU,
		SOUND_MENU,
		CONTROLLER_MENU,
		SAVE_OPTIONS,
		CGI_MENU,
		CUSTOM_FILTER
	};
};

// save options to file
void save_ui_options(running_machine &machine);

#endif /* __UI_OPTSMENU_H__ */
