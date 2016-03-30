// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
//============================================================
//
//  chainmanager.h - BGFX shader chain manager
//
//  Provides loading for BGFX shader effect chains, defined
//  by chain.h and read by chainreader.h
//
//============================================================

#pragma once

#ifndef __DRAWBGFX_CHAIN_MANAGER__
#define __DRAWBGFX_CHAIN_MANAGER__

#include <vector>
#include <string>

#include "texturemanager.h"
#include "targetmanager.h"
#include "effectmanager.h"

class running_machine;

class bgfx_chain;

class chain_manager {
public:
	chain_manager(osd_options& options, texture_manager& textures, target_manager& targets, effect_manager& effects)
		: m_options(options)
		, m_textures(textures)
		, m_targets(targets)
		, m_effects(effects)
	{
	}
	~chain_manager();

	// Getters
	bgfx_chain* chain(std::string name, running_machine& machine, uint32_t window_index, uint32_t screen_index);

private:
	bgfx_chain* load_chain(std::string name, running_machine& machine, uint32_t window_index, uint32_t screen_index);

	osd_options&                        m_options;
	texture_manager&                    m_textures;
	target_manager&                     m_targets;
	effect_manager&                     m_effects;
	std::vector<bgfx_chain*>            m_chains;
};

#endif // __DRAWBGFX_CHAIN_MANAGER__
