#pragma once

#ifndef __RENDER_BGFX__
#define __RENDER_BGFX__

#include <bgfx/bgfx.h>

#include <map>
#include <vector>

#include "binpacker.h"

class texture_manager;
class target_manager;
class shader_manager;
class effect_manager;
class bgfx_texture;
class bgfx_effect;
class bgfx_target;

/* sdl_info is the information about SDL for the current screen */
class renderer_bgfx : public osd_renderer
{
public:
	renderer_bgfx(osd_window *w)
		: osd_renderer(w, FLAG_NONE)
		, m_dimensions(0, 0)
	{
	}
	virtual ~renderer_bgfx();

	static bool init(running_machine &machine) { return false; }
	static void exit();

	virtual int create() override;
	virtual slider_state* get_slider_list() override;
	virtual int draw(const int update) override;
#ifdef OSD_SDL
	virtual int xy_to_render_target(const int x, const int y, int *xt, int *yt) override;
#else
	virtual void save() override { }
	virtual void record() override { }
	virtual void toggle_fsfx() override { }
#endif
	virtual render_primitive_list *get_primitives() override
	{
		osd_dim wdim = window().get_size();
		window().target()->set_bounds(wdim.width(), wdim.height(), window().aspect());
		return &window().target()->get_primitives();
	}

private:
	struct ScreenVertex
	{
		float m_x;
		float m_y;
		float m_z;
		UINT32 m_rgba;
		float m_u;
		float m_v;

		static void init()
		{
			ms_decl.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.end();
		}

		static bgfx::VertexDecl ms_decl;
	};

	void allocate_buffer(render_primitive *prim, UINT32 blend, bgfx::TransientVertexBuffer *buffer);
	enum buffer_status
	{
		BUFFER_PRE_FLUSH,
		BUFFER_FLUSH,
		BUFFER_EMPTY,
		BUFFER_DONE
	};
	buffer_status buffer_primitives(int view, bool atlas_valid, render_primitive** prim, bgfx::TransientVertexBuffer* buffer);

	void render_textured_quad(int view, render_primitive* prim, bgfx::TransientVertexBuffer* buffer);

	void put_packed_quad(render_primitive *prim, UINT32 hash, ScreenVertex* vertex);
	void put_polygon(const float* coords, UINT32 num_coords, float r, UINT32 rgba, ScreenVertex* vertex);
	void put_line(float x0, float y0, float x1, float y1, float r, UINT32 rgba, ScreenVertex* vertex, float fth = 1.0f);

	void set_bgfx_state(UINT32 blend);

	uint32_t u32Color(uint32_t r, uint32_t g, uint32_t b, uint32_t a);

	bool check_for_dirty_atlas();
	bool update_atlas();
	void process_atlas_packs(std::vector<std::vector<rectangle_packer::packed_rectangle>>& packed);
	const bgfx::Memory* mame_texture_data_to_bgfx_texture_data(UINT32 format, int width, int height, int rowpixels, const rgb_t *palette, void *base);
	UINT32 get_texture_hash(render_primitive *prim);

	bgfx_target* m_framebuffer;
	bgfx_texture* m_texture_cache;

	// Original display_mode
	osd_dim m_dimensions;

	texture_manager* m_textures;
	target_manager* m_targets;
	shader_manager* m_shaders;
	effect_manager* m_effects;
	bgfx_effect* m_gui_effect[4];
	bgfx_effect* m_screen_effect[4];

	std::map<UINT32, rectangle_packer::packed_rectangle> m_hash_to_entry;
	std::vector<rectangle_packer::packable_rectangle> m_texinfo;
	rectangle_packer m_packer;

	uint32_t m_width[16];
	uint32_t m_height[16];
	uint32_t m_white[16*16];
	enum : uint16_t { CACHE_SIZE = 1024 };
	enum : uint32_t { PACKABLE_SIZE = 128 };
	enum : UINT32 { WHITE_HASH = 0x87654321 };

	static bool s_window_set;
};

#endif
