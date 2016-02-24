// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
//============================================================
//
//  texturemanager.h - BGFX texture manager
//
//  Maintains a string-to-entry mapping for any registered
//  textures.
//
//============================================================

#pragma once

#ifndef __DRAWBGFX_TEXTURE_MANAGER__
#define __DRAWBGFX_TEXTURE_MANAGER__

#include <map>
#include <string>

#include <bgfx/bgfx.h>

class bgfx_texture;

class texture_manager {
public:
	texture_manager() { }
	~texture_manager();

	bgfx_texture* create_texture(std::string name, bgfx::TextureFormat::Enum format, uint32_t width, uint32_t height, void* data = nullptr, uint32_t flags = BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP);
	void add_texture(std::string name, bgfx_texture* texture);

	// Getters
	bgfx_texture* texture(std::string name);

private:
	bgfx_texture* create_texture(std::string name);

	std::map<std::string, bgfx_texture*> m_textures;
};

#endif // __DRAWBGFX_TEXTURE_MANAGER__
