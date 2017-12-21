/*
 * \brief  SDL-based implementation of the Genode framebuffer
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <framebuffer_session/framebuffer_session.h>
#include <input/root.h>
#include <timer_session/connection.h>

/* Linux includes */
#include <SDL/SDL.h>

/* local includes */
#include "input.h"

namespace Framebuffer {
	class Session_component;
	using namespace Genode;
}


namespace Fb_sdl {
	class Main;
	using namespace Genode;
}


class Framebuffer::Session_component : public Rpc_object<Session>
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		SDL_Surface *_screen { nullptr };

		Mode                  _mode;
		Dataspace_capability  _fb_ds_cap;
		void                 *_fb_ds_addr;

		Timer::Connection _timer;

	public:

		/**
		 * Constructor
		 */
		Session_component(Env &env, Framebuffer::Mode mode,
		                  Dataspace_capability fb_ds_cap, void *fb_ds_addr)
		:
			_mode(mode), _fb_ds_cap(fb_ds_cap), _fb_ds_addr(fb_ds_addr), _timer(env)
		{ }

		void screen(SDL_Surface *screen) { _screen = screen; }

		Dataspace_capability dataspace() override { return _fb_ds_cap; }

		Mode mode() const override { return _mode; }

		void mode_sigh(Signal_context_capability) override { }

		void sync_sigh(Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			if (sigh.valid())
				_timer.trigger_periodic(100000000 / 5994); /* 59.94Hz */
		}

		void refresh(int x, int y, int w, int h) override
		{
			/* clip refresh area to screen boundaries */
			int x1 = max(x, 0);
			int y1 = max(y, 0);
			int x2 = min(x + w - 1, _mode.width()  - 1);
			int y2 = min(y + h - 1, _mode.height() - 1);

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


struct Fb_sdl::Main
{
	/* fatal exceptions */
	struct Sdl_init_failed               : Exception { };
	struct Sdl_videodriver_not_supported : Exception { };
	struct Sdl_setvideomode_failed       : Exception { };

	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	int const _fb_width  = _config.xml().attribute_value("width", 1024UL);
	int const _fb_height = _config.xml().attribute_value("height", 768UL);

	Framebuffer::Mode _fb_mode { _fb_width, _fb_height, Framebuffer::Mode::RGB565 };

	Attached_ram_dataspace _fb_ds { _env.ram(), _env.rm(),
	                                _fb_mode.width()*_fb_mode.height()*_fb_mode.bytes_per_pixel() };

	Framebuffer::Session_component _fb_session { _env, _fb_mode, _fb_ds.cap(), _fb_ds.local_addr<void>() };

	Static_root<Framebuffer::Session> _fb_root { _env.ep().manage(_fb_session) };

	Input::Session_component _input_session { _env, _env.ram() };
	Input::Root_component    _input_root    { _env.ep().rpc_ep(), _input_session };

	Input::Handler_component _input_handler_component { _input_session };
	Input::Handler_client    _input_handler_client    { _env.ep().manage(_input_handler_component) };

	Main(Env &env) : _env(env)
	{
		/*
		 * Initialize libSDL window
		 */
		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			error("SDL_Init failed (", Genode::Cstring(SDL_GetError()), ")");
			throw Sdl_init_failed();
		}

		/*
		 * We're testing only X11.
		 */
		char driver[16] = { 0 };
		SDL_VideoDriverName(driver, sizeof(driver));
		if (::strcmp(driver, "x11") != 0) {
			error("fb_sdl works on X11 only. "
			      "Your SDL backend is ", Genode::Cstring(driver), ".");
			throw Sdl_videodriver_not_supported();
		}

		SDL_Surface *screen = SDL_SetVideoMode(_fb_mode.width(), _fb_mode.height(),
		                                       _fb_mode.bytes_per_pixel()*8, SDL_SWSURFACE);
		if (!screen) {
			error("SDL_SetVideoMode failed (", Genode::Cstring(SDL_GetError()), ")");
			throw Sdl_setvideomode_failed();
		}
		_fb_session.screen(screen);

		SDL_ShowCursor(0);

		log("creating virtual framebuffer for mode ", _fb_mode);

		_env.parent().announce(env.ep().manage(_fb_root));
		_env.parent().announce(env.ep().manage(_input_root));

		init_input_backend(_env, _input_handler_client);
	}
};


void Component::construct(Genode::Env &env) { static Fb_sdl::Main inst(env); }
