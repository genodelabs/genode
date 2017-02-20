/*
 * \brief  Nitpicker-based virtual framebuffer
 * \author Norman Feske
 * \date   2006-09-21
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/heap.h>
#include <base/rpc_server.h>
#include <base/attached_rom_dataspace.h>
#include <input/component.h>
#include <scout/user_state.h>
#include <scout/nitpicker_graphics_backend.h>

#include "framebuffer_window.h"
#include "services.h"


static Genode::Allocator *_alloc_ptr;

void *operator new (__SIZE_TYPE__ n) { return _alloc_ptr->alloc(n); }


void Scout::Launcher::launch() { }


class Background_animator : public Scout::Tick
{
	private:

		Framebuffer_window<Scout::Pixel_rgb565> &_fb_win;

		int _bg_offset = 0;

	public:

		/**
		 * Constructor
		 */
		Background_animator(Framebuffer_window<Scout::Pixel_rgb565> &fb_win)
		: _fb_win(fb_win) { schedule(20); }

		/**
		 * Tick interface
		 */
		int on_tick()
		{
			_fb_win.bg_offset(_bg_offset);
			_bg_offset += 2;
			_fb_win.refresh();

			/* schedule next tick */
			return 1;
		}
};


/**
 * Animated background
 */
static bool config_animate = true;
static bool config_alpha   = true;

/**
 * Size and position of virtual frame buffer
 */
static unsigned long config_fb_width  = 500;
static unsigned long config_fb_height = 400;
static long          config_fb_x      = 400;
static long          config_fb_y      = 260;

/**
 * Window title
 */
static const char *config_title = "Liquid Framebuffer";

/**
 * Resize handle
 */
static bool config_resize_handle = false;

/**
 * Window decoration
 */
static bool config_decoration = true;

/**
 * Parse configuration
 */
static void read_config(Genode::Xml_node config_node)
{
	using namespace Genode;

	try {
		char buf[16];
		config_node.attribute("animate").value(buf, sizeof(buf));

		if      (!strcmp("off", buf)) config_animate = false;
		else if (!strcmp("on",  buf)) config_animate = true;
		else
			Genode::printf("Warning: invalid value for animate declaration,\n"
			               "         valid values are 'on', 'off.\n'");
	} catch (Xml_node::Nonexistent_attribute) { }

	config_alpha = config_animate;

	try { config_node.attribute("xpos").value(&config_fb_x); }
	catch (Xml_node::Nonexistent_attribute) { }

	try { config_node.attribute("ypos").value(&config_fb_y); }
	catch (Xml_node::Nonexistent_attribute) { }

	try { config_node.attribute("width").value(&config_fb_width); }
	catch (Xml_node::Nonexistent_attribute) { }

	try { config_node.attribute("height").value(&config_fb_height); }
	catch (Xml_node::Nonexistent_attribute) { }

	try {
		static char buf[64];
		config_node.attribute("title").value(buf, sizeof(buf));
		config_title = buf;
	} catch (Xml_node::Nonexistent_attribute) { }

	try {
		char buf[16];
		config_node.attribute("decoration").value(buf, sizeof(buf));

		if      (!strcmp("off", buf)) config_decoration = false;
		else if (!strcmp("on",  buf)) config_decoration = true;
		else
			Genode::printf("Warning: invalid value for decoration declaration,\n"
			               "         valid values are 'on', 'off.\n'");
	} catch (Xml_node::Nonexistent_attribute) { }

	try {
		char buf[16];
		config_node.attribute("resize_handle").value(buf, sizeof(buf));

		if      (!strcmp("off", buf)) config_resize_handle = false;
		else if (!strcmp("on",  buf)) config_resize_handle = true;
		else
			Genode::printf("Warning: invalid value for resize_handle declaration,\n"
			               "         valid values are 'on', 'off.\n'");
	} catch (Xml_node::Nonexistent_attribute) { }

}


/*******************
 ** Input handler **
 *******************/

struct Input_handler
{
	GENODE_RPC(Rpc_handle_input, void, handle_input, Scout::Event const &);
	GENODE_RPC_INTERFACE(Rpc_handle_input);
};


namespace Liquid_fb {
	class Main;
	using namespace Scout;
	using namespace Genode;
}


class Liquid_fb::Main : public Scout::Event_handler
{
	private:

		Env &_env;

		Heap _heap { _env.ram(), _env.rm() };

		bool const _global_new_initialized = (_alloc_ptr = &_heap, true);

		Attached_rom_dataspace _config { _env, "config" };

		void _process_config()
		{
			try { read_config(_config.xml()); } catch (...) { }
		}

		bool const _config_processed = (_process_config(), true);

		/* heuristic for allocating the double-buffer backing store */
		enum { WINBORDER_WIDTH = 10, WINBORDER_HEIGHT = 40 };

		Nitpicker::Connection _nitpicker { _env };

		Platform _platform { _env, *_nitpicker.input() };

		bool const _event_handler_registered = (_platform.event_handler(*this), true);

		Scout::Area  const _max_size { config_fb_width  + WINBORDER_WIDTH,
		                               config_fb_height + WINBORDER_HEIGHT };
		Scout::Point const _initial_position { config_fb_x, config_fb_y };
		Scout::Area  const _initial_size = _max_size;

		Nitpicker_graphics_backend
			_graphics_backend { _env.rm(), _nitpicker, _heap, _max_size,
			                    _initial_position, _initial_size };

		Input::Session_component _input_session_component { _env, _env.ram() };

		bool const _window_content_initialized =
			(init_window_content(_env.ram(), _env.rm(), _heap, _input_session_component,
			                     config_fb_width, config_fb_height, config_alpha), true);

		Framebuffer_window<Pixel_rgb565>
			_fb_win { _graphics_backend, window_content(),
			          _initial_position, _initial_size, _max_size,
			          config_title, config_alpha,
			          config_resize_handle, config_decoration };

		/* create background animator if configured */
		Constructible<Background_animator> _fb_win_bg_anim;
		void _init_background_animator()
		{
			if (config_animate) {
				_fb_win_bg_anim.construct(_fb_win);
			}
		}
		bool _background_animator_initialized = (_init_background_animator(), true);

		User_state _user_state { &_fb_win, &_fb_win,
		                         _initial_position.x(), _initial_position.y() };

		void _init_fb_win()
		{
			_fb_win.parent(&_user_state);
			_fb_win.content_geometry(config_fb_x, config_fb_y,
			                         config_fb_width, config_fb_height);
		}

		bool _fb_win_initialized = (_init_fb_win(), true);

		bool _services_initialized = (init_services(_env, _input_session_component), true);

		unsigned long _curr_time = _platform.timer_ticks();
		unsigned long _old_time  = _curr_time;

		void _handle_config()
		{
			_config.update();

			/* keep the current values by default */
			config_fb_x      = _fb_win.view_x();
			config_fb_y      = _fb_win.view_y();
			config_fb_width  = _fb_win.view_w();
			config_fb_height = _fb_win.view_h();

			try { read_config(_config.xml()); } catch (...) { }
			
			_fb_win.name(config_title);
			_fb_win.config_alpha(config_alpha);
			_fb_win.config_resize_handle(config_resize_handle);
			_fb_win.config_decoration(config_decoration);

			/* must get called after 'config_decoration()' */
			_fb_win.content_geometry(config_fb_x, config_fb_y,
			 config_fb_width, config_fb_height);
			_user_state.update_view_offset();
		}

		Signal_handler<Main> _config_handler {
			_env.ep(), *this, &Main::_handle_config };

	public:

		void handle_event(Scout::Event const &event) override
		{
			using Scout::Event;

			Event ev = event;

			if (ev.type != Event::WHEEL)
				ev.mouse_position = ev.mouse_position - _user_state.view_position();

			/* direct all keyboard events to the window content */
			if ((ev.type == Event::PRESS || ev.type == Event::RELEASE)
			 && (ev.code != Event::BTN_LEFT))
				window_content()->handle_event(ev);
			else
				_user_state.handle_event(ev);

			if (ev.type == Event::TIMER) {
				Scout::Tick::handle(_platform.timer_ticks());
			}

			/* perform periodic redraw */
			_curr_time = _platform.timer_ticks();
			if ((_curr_time - _old_time > 20) || (_curr_time < _old_time)) {
				_old_time = _curr_time;
				_fb_win.process_redraw();
			}
		}

		Main(Env &env) : _env(env)
		{
			_config.sigh(_config_handler);
		}
};


void Component::construct(Genode::Env &env) { static Liquid_fb::Main main(env); }
