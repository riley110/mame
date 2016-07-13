// license:BSD-3-Clause
// copyright-holders:Olivier Galibert, R. Belmont, Couriersud
//============================================================
//
//  osdwindow.h - SDL window handling
//
//============================================================

#ifndef __OSDWINDOW__
#define __OSDWINDOW__

#include "emu.h"
#include "render.h"
#include "osdhelper.h"
#include "../frontend/mame/ui/menuitem.h"

// standard windows headers
#ifdef OSD_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#endif

#ifdef OSD_SDL
// forward declaration
struct SDL_Window;
#endif

//============================================================
//  TYPE DEFINITIONS
//============================================================

class osd_options;
class render_primitive_list;

enum
{
	VIDEO_MODE_NONE = 0,
	VIDEO_MODE_GDI,
	VIDEO_MODE_BGFX,
#if (USE_OPENGL)
	VIDEO_MODE_OPENGL,
#endif
	VIDEO_MODE_SDL2ACCEL,
	VIDEO_MODE_D3D,
	VIDEO_MODE_SOFT,

	VIDEO_MODE_COUNT
};

class osd_monitor_info
{
public:
	osd_monitor_info(void *handle, const char *monitor_device, float aspect)
	: m_is_primary(false), m_name(monitor_device), m_handle(handle), m_aspect(aspect)
	{
	}

	virtual ~osd_monitor_info() { }

	virtual void refresh() = 0;

	const void *oshandle() const { return m_handle; }

	const osd_rect &position_size() const { return m_pos_size; }
	const osd_rect &usuable_position_size() const { return m_usuable_pos_size; }

	const char *devicename() const { return m_name.length() ? m_name.c_str() : "UNKNOWN"; }

	float aspect() const { return m_aspect; }
	float pixel_aspect() const { return m_aspect / ((float)m_pos_size.width() / (float)m_pos_size.height()); }

	void update_resolution(const int new_width, const int new_height) const { m_pos_size.resize(new_width, new_height); }
	void set_aspect(const float a) { m_aspect = a; }
	bool is_primary() const { return m_is_primary; }

	static std::shared_ptr<osd_monitor_info> pick_monitor(osd_options &options, int index);

	static std::list<std::shared_ptr<osd_monitor_info>> list;

protected:
	osd_rect            m_pos_size;
	osd_rect            m_usuable_pos_size;
	bool                m_is_primary;
	std::string         m_name;
private:
	void *              m_handle;                 // handle to the monitor
	float               m_aspect;                 // computed/configured aspect ratio of the physical device
};

class osd_window_config
{
public:
	osd_window_config() : aspect(0.0f), width(0), height(0), depth(0), refresh(0) {}

	float               aspect;                     // decoded aspect ratio FIXME: not used on windows
	int                 width;                      // decoded width
	int                 height;                     // decoded height
	int                 depth;                      // decoded depth - only SDL
	int                 refresh;                    // decoded refresh
};

class osd_renderer;

class osd_window : public std::enable_shared_from_this<osd_window>
{
public:
	osd_window(const osd_window_config &config)
	:
#ifndef OSD_SDL
		m_dc(nullptr), m_resize_state(0),
#endif
		m_primlist(nullptr),
		m_win_config(config),
		m_index(0),
		m_prescale(1),
		m_platform_window(nullptr),
		m_renderer(nullptr),
		m_main(nullptr)
		{}

	virtual render_target *target() = 0;
	virtual int fullscreen() const = 0;
	virtual running_machine &machine() const = 0;

	osd_renderer &renderer() const { return *m_renderer; }
	void set_renderer(std::unique_ptr<osd_renderer> renderer)
	{
		m_renderer = std::move(renderer);
	}

	int prescale() const { return m_prescale; };

	float pixel_aspect() const { return monitor()->pixel_aspect(); }

	bool swap_xy()
	{
		bool orientation_swap_xy =
			(machine().system().flags & ORIENTATION_SWAP_XY) == ORIENTATION_SWAP_XY;
		bool rotation_swap_xy =
			(target()->orientation() & ORIENTATION_SWAP_XY) == ORIENTATION_SWAP_XY;
		return orientation_swap_xy ^ rotation_swap_xy;
	};

	virtual osd_dim get_size() = 0;

	virtual osd_monitor_info *monitor() const = 0;

	template <class TWindow>
	TWindow platform_window() const { return static_cast<TWindow>(m_platform_window); }

	void set_platform_window(void *window)
	{
		assert(window == nullptr || m_platform_window == nullptr);
		m_platform_window = window;
	}

	std::shared_ptr<osd_window> main_window() const { return m_main;    }
	void set_main_window(std::shared_ptr<osd_window> main) { m_main = main; }

	// Clips the pointer to the bounds of this window
	virtual void capture_pointer() = 0;

	// Releases the pointer from a previously captured state
	virtual void release_pointer() = 0;

	virtual void show_pointer() = 0;
	virtual void hide_pointer() = 0;

	virtual void update() = 0;
	virtual void destroy() = 0;

	void renderer_reset() { m_renderer.reset(); }
#ifndef OSD_SDL
	virtual bool win_has_menu() = 0;
	// FIXME: cann we replace winwindow_video_window_monitor(nullptr) with monitor() ?
	virtual std::shared_ptr<osd_monitor_info> winwindow_video_window_monitor(const osd_rect *proposed) = 0;

	HDC                     m_dc;       // only used by GDI renderer!

	int                     m_resize_state;
#endif

	render_primitive_list   *m_primlist;
	osd_window_config       m_win_config;
	int                     m_index;
protected:
	int                     m_prescale;
private:
	void                           *m_platform_window;
	std::unique_ptr<osd_renderer>  m_renderer;
	std::shared_ptr<osd_window>    m_main;
};

class osd_renderer
{
public:

	/* Generic flags */
	static const int FLAG_NONE                  = 0x0000;
	static const int FLAG_NEEDS_OPENGL          = 0x0001;
	static const int FLAG_HAS_VECTOR_SCREEN     = 0x0002;

	/* SDL 1.2 flags */
	static const int FLAG_NEEDS_DOUBLEBUF       = 0x0100;
	static const int FLAG_NEEDS_ASYNCBLIT       = 0x0200;

	osd_renderer(std::shared_ptr<osd_window> window, const int flags)
	: m_sliders_dirty(false), m_window(window), m_flags(flags) { }

	virtual ~osd_renderer() { }

	std::shared_ptr<osd_window> assert_window() const
	{
		auto win = m_window.lock();
		assert_always(win != nullptr, "Window weak_ptr is not available!");
		return win;
	}

	std::shared_ptr<osd_window> try_getwindow() const
	{
		return m_window.lock();
	}

	bool has_flags(const int flag) const { return ((m_flags & flag)) == flag; }
	void set_flags(int aflag) { m_flags |= aflag; }
	void clear_flags(int aflag) { m_flags &= ~aflag; }

	void notify_changed() { set_flags(FI_CHANGED); }

	/* Interface to be implemented by render code */

	virtual int create() = 0;
	virtual render_primitive_list *get_primitives() = 0;

	virtual void add_audio_to_recording(const INT16 *buffer, int samples_this_frame) { }
	virtual std::vector<ui::menu_item> get_slider_list() { return m_sliders; }
	virtual int draw(const int update) = 0;
	virtual int xy_to_render_target(const int x, const int y, int *xt, int *yt) { return 0; };
	virtual void save() { };
	virtual void record() { };
	virtual void toggle_fsfx() { };
	virtual bool sliders_dirty() { return m_sliders_dirty; }

	static std::unique_ptr<osd_renderer> make_for_type(int mode, std::shared_ptr<osd_window> window, int extra_flags = FLAG_NONE);

protected:
	virtual void build_slider_list() { }

	/* Internal flags */
	static const int FI_CHANGED                 = 0x010000;
	bool                        m_sliders_dirty;
	std::vector<ui::menu_item>   m_sliders;

private:
	std::weak_ptr<osd_window>  m_window;
	int         m_flags;
};



//============================================================
//  CONSTANTS
//============================================================

#define MAX_VIDEO_WINDOWS           (4)

#define VIDEO_SCALE_MODE_NONE       (0)

#define GLSL_SHADER_MAX 10


//============================================================
//  TYPE DEFINITIONS
//============================================================

struct osd_video_config
{
	// global configuration
	int                 windowed;                   // start windowed?
	int                 prescale;                   // prescale factor
	int                 keepaspect;                 // keep aspect ratio
	int                 numscreens;                 // number of screens
	int                 fullstretch;                // fractional stretch

	// hardware options
	int                 mode;                       // output mode
	int                 waitvsync;                  // spin until vsync
	int                 syncrefresh;                // sync only to refresh rate
	int                 switchres;                  // switch resolutions

	// d3d, accel, opengl
	int                 filter;                     // enable filtering
	//int                 filter;         // enable filtering, disabled if glsl_filter>0

	// OpenGL options
	int                 glsl;
	int                 glsl_filter;        // glsl filtering, >0 disables filter
	char *              glsl_shader_mamebm[GLSL_SHADER_MAX]; // custom glsl shader set, mame bitmap
	int                 glsl_shader_mamebm_num; // custom glsl shader set number, mame bitmap
	char *              glsl_shader_scrn[GLSL_SHADER_MAX]; // custom glsl shader set, screen bitmap
	int                 glsl_shader_scrn_num; // custom glsl shader number, screen bitmap
	int                 pbo;
	int                 vbo;
	int                 allowtexturerect;   // allow GL_ARB_texture_rectangle, default: no
	int                 forcepow2texture;   // force power of two textures, default: no

	// dd, d3d
	int                 triplebuf;                  // triple buffer

	//============================================================
	// SDL - options
	//============================================================
	int                 novideo;                // don't draw, for pure CPU benchmarking

	int                 centerh;
	int                 centerv;

	// vector options
	float               beamwidth;      // beam width

	// perftest
	int                 perftest;       // print out real video fps

	// X11 options
	int                 restrictonemonitor; // in fullscreen, confine to Xinerama monitor 0

	// YUV options
	int                 scale_mode;
};

//============================================================
//  GLOBAL VARIABLES
//============================================================

extern osd_video_config video_config;

#endif /* __OSDWINDOW__ */
