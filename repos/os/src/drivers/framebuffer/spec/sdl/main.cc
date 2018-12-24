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
#include <blit/blit.h>

/* Linux includes */
#include <SDL/SDL.h>

/* local includes */
#include "convert_keycode.h"

namespace Framebuffer {
	class Session_component;
	using namespace Genode;
}


namespace Fb_sdl {
	class Main;
	using namespace Genode;
}


/* fatal exceptions */
struct Sdl_init_failed               : Genode::Exception { };
struct Sdl_videodriver_not_supported : Genode::Exception { };
struct Sdl_setvideomode_failed       : Genode::Exception { };


class Framebuffer::Session_component : public Rpc_object<Session>
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		Env &_env;

		Mode         _next_mode;
		Mode mutable _requested_mode = _next_mode;
		Mode         _mode           = _next_mode;

		SDL_Surface *_screen { nullptr };

		Constructible<Attached_ram_dataspace> _fb_ds { };

		Signal_context_capability _mode_sigh { };
		Signal_context_capability _sync_sigh { };

	public:

		void submit_sync()
		{
			if (_sync_sigh.valid())
				Signal_transmitter(_sync_sigh).submit();
		}

		void submit_mode_change(Mode next_mode)
		{
			_next_mode = next_mode;

			if (_mode_sigh.valid())
				Signal_transmitter(_mode_sigh).submit();
		}

		/**
		 * Constructor
		 */
		Session_component(Env &env, Framebuffer::Mode next_mode)
		:
			_env(env), _next_mode(next_mode)
		{ }

		Dataspace_capability dataspace() override
		{
			unsigned const bpp   = _requested_mode.bytes_per_pixel();
			unsigned const flags = SDL_SWSURFACE | SDL_RESIZABLE;
			unsigned const w     = _requested_mode.width();
			unsigned const h     = _requested_mode.height();
			
			if (SDL_VideoModeOK(w, h, bpp*8, flags))
				_screen = SDL_SetVideoMode(w, h, bpp*8, flags);

			if (!_screen) {
				error("SDL_SetVideoMode failed (", Genode::Cstring(SDL_GetError()), ")");
				throw Sdl_setvideomode_failed();
			}

			/*
			 * Preserve content of old dataspace in new SDL screen to reduce
			 * flickering during resize.
			 *
			 * Note that flickering cannot fully be avoided because the host
			 * window is immediately cleared by 'SDL_SetVideoMode'.
			 */
			refresh(0, 0, w, h);

			_mode = _requested_mode;

			/*
			 * Allocate new dataspace, and initialize with the original pixels.
			 */
			_fb_ds.construct(_env.ram(), _env.rm(), w*h*bpp);

			blit(_screen->pixels, _screen->pitch, _fb_ds->local_addr<char>(), bpp*w,
			     min(w, (unsigned)_screen->w)*bpp,
			     min(h, (unsigned)_screen->h));

			return _fb_ds->cap();
		}

		Mode mode() const override
		{
			_requested_mode = _next_mode;
			return _requested_mode;
		}

		void mode_sigh(Signal_context_capability sigh) override
		{
			_mode_sigh = sigh;
		}

		void sync_sigh(Signal_context_capability sigh) override
		{
			_sync_sigh = sigh;
		}

		void refresh(int x, int y, int w, int h) override
		{
			if (!_fb_ds.constructed())
				return;

			/* clip refresh area to screen boundaries */
			int const x1 = max(x, 0);
			int const y1 = max(y, 0);
			int const x2 = min(x + w - 1, min(_mode.width(),  _screen->w) - 1);
			int const y2 = min(y + h - 1, min(_mode.height(), _screen->h) - 1);

			if (x1 > x2 || y1 > y2)
				return;

			/* blit pixels from framebuffer dataspace to SDL screen */
			unsigned const bpp = _mode.bytes_per_pixel();

			char const * const src_base  = _fb_ds->local_addr<char>();
			unsigned     const src_pitch = bpp*_mode.width();
			char const * const src       = src_base + y1*src_pitch + bpp*x1;

			unsigned     const dst_pitch =         _screen->pitch;
			char       * const dst_base  = (char *)_screen->pixels;
			char       * const dst       = dst_base + y1*dst_pitch + bpp*x1;

			blit(src, src_pitch, dst, dst_pitch, bpp*(x2 - x1 + 1), y2 - y1 + 1);

			/* flush pixels in sdl window */
			SDL_UpdateRect(_screen, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
		}
};


struct Fb_sdl::Main
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Timer::Connection _timer { _env };

	int _fb_width  = _config.xml().attribute_value("width", 1024UL);
	int _fb_height = _config.xml().attribute_value("height", 768UL);

	Framebuffer::Mode _fb_mode { _fb_width, _fb_height, Framebuffer::Mode::RGB565 };

	Framebuffer::Session_component _fb_session { _env, _fb_mode };

	Static_root<Framebuffer::Session> _fb_root { _env.ep().manage(_fb_session) };

	Input::Session_component _input_session { _env, _env.ram() };
	Input::Root_component    _input_root    { _env.ep().rpc_ep(), _input_session };

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	int _mx = 0, _my = 0;

	void _handle_sdl_event(SDL_Event const &event);
	void _handle_sdl_events();

	void _handle_timer()
	{
		_handle_sdl_events();
		_fb_session.submit_sync();
	}

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

		SDL_ShowCursor(0);

		_env.parent().announce(env.ep().manage(_fb_root));
		_env.parent().announce(env.ep().manage(_input_root));

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(100000000 / 5994); /* 59.94Hz */
	}
};


void Fb_sdl::Main::_handle_sdl_event(SDL_Event const &event)
{
	using namespace Input;

	if (event.type == SDL_VIDEORESIZE) {

		Framebuffer::Mode const mode(event.resize.w, event.resize.h,
		                             Framebuffer::Mode::RGB565);

		_fb_session.submit_mode_change(mode);
		return;
	}

	/* query new mouse position */
	if (event.type == SDL_MOUSEMOTION) {
		int ox = _mx, oy = _my;
		SDL_GetMouseState(&_mx, &_my);

		/* drop superficial events */
		if (ox == _mx && oy == _my)
			return;

		_input_session.submit(Absolute_motion{_mx, _my});
		return;
	}

	/* determine key code */
	Keycode keycode = KEY_UNKNOWN;
	switch (event.type) {
	case SDL_KEYUP:
	case SDL_KEYDOWN:

		keycode = convert_keycode(event.key.keysym.sym);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:

		switch (event.button.button) {
		case SDL_BUTTON_LEFT:   keycode = BTN_LEFT;   break;
		case SDL_BUTTON_MIDDLE: keycode = BTN_MIDDLE; break;
		case SDL_BUTTON_RIGHT:  keycode = BTN_RIGHT;  break;
		default: break;
		}
	}

	/* determine event type */
	switch (event.type) {

	case SDL_KEYUP:
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_WHEELUP
		 || event.button.button == SDL_BUTTON_WHEELDOWN)
			/* ignore */
			return;

		_input_session.submit(Release{keycode});
		return;

	case SDL_KEYDOWN:
	case SDL_MOUSEBUTTONDOWN:

		if (event.button.button == SDL_BUTTON_WHEELUP)
			_input_session.submit(Wheel{0, 1});
		else if (event.button.button == SDL_BUTTON_WHEELDOWN)
			_input_session.submit(Wheel{0, -1});
		else
			_input_session.submit(Press{keycode});
		return;

	default:
		break;
	}
}


void Fb_sdl::Main::_handle_sdl_events()
{
	SDL_Event event { };
	while (SDL_PollEvent(&event))
		_handle_sdl_event(event);
}


void Component::construct(Genode::Env &env) { static Fb_sdl::Main inst(env); }
