/*
 * \brief  SDL-based implementation of the Genode framebuffer
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <SDL/SDL.h>

/* Genode includes */
#include <util/misc_math.h>
#include <base/component.h>
#include <base/rpc_server.h>
#include <framebuffer_session/framebuffer_session.h>
#include <cap_session/connection.h>
#include <input/root.h>
#include <os/config.h>
#include <timer_session/connection.h>

/* local includes */
#include "input.h"


using Genode::Attached_ram_dataspace;


namespace Framebuffer { class Session_component; }

class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		SDL_Surface *_screen { nullptr };

		Mode                          _mode;
		Genode::Dataspace_capability  _fb_ds_cap;
		void                         *_fb_ds_addr;

		Timer::Connection _timer;

	public:

		/**
		 * Constructor
		 */
		Session_component(Framebuffer::Mode mode,
		                  Genode::Dataspace_capability fb_ds_cap, void *fb_ds_addr)
		:
			_mode(mode), _fb_ds_cap(fb_ds_cap), _fb_ds_addr(fb_ds_addr)
		{ }

		void screen(SDL_Surface *screen) { _screen = screen; }

		Genode::Dataspace_capability dataspace() override { return _fb_ds_cap; }

		Mode mode() const override { return _mode; }

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override
		{
			/* clip refresh area to screen boundaries */
			int x1 = Genode::max(x, 0);
			int y1 = Genode::max(y, 0);
			int x2 = Genode::min(x + w - 1, _mode.width()  - 1);
			int y2 = Genode::min(y + h - 1, _mode.height() - 1);

			if (x1 <= x2 && y1 <= y2) {

				/* copy pixels from shared dataspace to sdl surface */
				const int start_offset = _mode.bytes_per_pixel()*(y1*_mode.width() + x1);
				const int line_len     = _mode.bytes_per_pixel()*(x2 - x1 + 1);
				const int pitch        = _mode.bytes_per_pixel()*_mode.width();

				char *src = (char *)_fb_ds_addr     + start_offset;
				char *dst = (char *)_screen->pixels + start_offset;

				for (int i = y1; i <= y2; i++, src += pitch, dst += pitch)
					Genode::memcpy(dst, src, line_len);

				/* flush pixels in sdl window */
				SDL_UpdateRect(_screen, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
			}
		}
};


namespace Input {

	struct Handler_rpc : Handler
	{
		GENODE_RPC(Rpc_event, void, event, Input::Event);
		GENODE_RPC_INTERFACE(Rpc_event);
	};

	struct Handler_client : Handler
	{
		Genode::Capability<Handler_rpc> cap;

		Handler_client(Genode::Capability<Handler_rpc> cap) : cap(cap) { }

		void event(Input::Event ev) override
		{
			cap.call<Handler_rpc::Rpc_event>(ev);
		}
	};

	struct Handler_component : Genode::Rpc_object<Handler_rpc, Handler_component>
	{
		Session_component &session;

		Handler_component(Session_component &session) : session(session) { }

		void event(Input::Event e) override { session.submit(e); }
	};
}


/**
 * Read integer value from config attribute
 */
template<typename T>
static T config_arg(char const *attr, T const &default_value)
{
	long value = default_value;

	try { Genode::config()->xml_node().attribute(attr).value(&value); }
	catch (...) { }

	return value;
}


struct Main
{
	/* fatal exceptions */
	struct Sdl_init_failed { };
	struct Sdl_videodriver_not_supported { };
	struct Sdl_setvideomode_failed { };

	Genode::Env &env;

	int fb_width  { config_arg("width",  1024) };
	int fb_height { config_arg("height", 768) };

	Framebuffer::Mode fb_mode { fb_width, fb_height, Framebuffer::Mode::RGB565 };

	Attached_ram_dataspace fb_ds { &env.ram(),
	                               fb_mode.width()*fb_mode.height()*fb_mode.bytes_per_pixel() };

	Framebuffer::Session_component fb_session { fb_mode, fb_ds.cap(), fb_ds.local_addr<void>() };

	Genode::Static_root<Framebuffer::Session> fb_root { env.ep().manage(fb_session) };

	Input::Session_component input_session { env, env.ram() };
	Input::Root_component    input_root    { env.ep().rpc_ep(), input_session };

	Input::Handler_component input_handler_component { input_session };
	Input::Handler_client    input_handler_client    { env.ep().manage(input_handler_component) };

	Main(Genode::Env &env) : env(env)
	{
		/*
		 * Initialize libSDL window
		 */
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			Genode::error("SDL_Init failed (", Genode::Cstring(SDL_GetError()), ")");
			throw Sdl_init_failed();
		}

		/*
		 * We're testing only X11.
		 */
		char driver[16] = { 0 };
		SDL_VideoDriverName(driver, sizeof(driver));
		if (::strcmp(driver, "x11") != 0) {
			Genode::error("fb_sdl works on X11 only. "
			              "Your SDL backend is ", Genode::Cstring(driver), ".");
			throw Sdl_videodriver_not_supported();
		}

		SDL_Surface *screen = SDL_SetVideoMode(fb_mode.width(), fb_mode.height(),
		                                       fb_mode.bytes_per_pixel()*8, SDL_SWSURFACE);
		if (!screen) {
			Genode::error("SDL_SetVideoMode failed (", Genode::Cstring(SDL_GetError()), ")");
			throw Sdl_setvideomode_failed();
		}
		fb_session.screen(screen);

		SDL_ShowCursor(0);

		Genode::log("creating virtual framebuffer for mode ", fb_mode);

		env.parent().announce(env.ep().manage(fb_root));
		env.parent().announce(env.ep().manage(input_root));

		init_input_backend(input_handler_client);
	}
};


void Component::construct(Genode::Env &env) { static Main inst(env); }
