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
#include <event_session/connection.h>
#include <capture_session/connection.h>
#include <timer_session/connection.h>

/* Linux includes */
#include <SDL/SDL.h>

/* local includes */
#include "convert_keycode.h"

namespace Fb_sdl {
	class Main;
	using namespace Genode;
}


/* fatal exceptions */
struct Sdl_init_failed               : Genode::Exception { };
struct Sdl_videodriver_not_supported : Genode::Exception { };
struct Sdl_setvideomode_failed       : Genode::Exception { };


struct Fb_sdl::Main
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Timer::Connection _timer { _env };
	Event::Connection _event { _env };

	using Area           = Capture::Area;
	using Point          = Capture::Area;
	using Pixel          = Capture::Pixel;
	using Affected_rects = Capture::Session::Affected_rects;
	using Event_batch    = Event::Session_client::Batch;

	void _init_sdl()
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
	}

	bool const _sdl_initialized = ( _init_sdl(), true );

	struct Sdl_screen
	{
		Area const size;

		SDL_Surface &_sdl_surface = _init_sdl_surface();

		SDL_Surface &_init_sdl_surface()
		{
			unsigned const bpp   = 32;
			unsigned const flags = SDL_SWSURFACE | SDL_RESIZABLE;

			SDL_Surface *surface_ptr = nullptr;

			if (SDL_VideoModeOK(size.w(), size.h(), bpp, flags))
				surface_ptr = SDL_SetVideoMode(size.w(), size.h(), bpp, flags);

			if (!surface_ptr) {
				error("SDL_SetVideoMode failed (", Genode::Cstring(SDL_GetError()), ")");
				throw Sdl_setvideomode_failed();
			}
			return *surface_ptr;
		}

		Sdl_screen(Area size) : size(size) { }

		template <typename FN>
		void with_surface(FN const &fn)
		{
			Surface<Pixel> surface { (Pixel *)_sdl_surface.pixels, size };
			fn(surface);
		}

		void flush(Capture::Rect rect)
		{
			SDL_UpdateRect(&_sdl_surface, rect.x1(), rect.y1(), rect.w(), rect.h());
		}
	};

	Constructible<Sdl_screen> _sdl_screen { };

	Capture::Connection _capture { _env };

	Constructible<Capture::Connection::Screen> _captured_screen { };

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	int _mx = 0, _my = 0;

	void _handle_sdl_event(Event_batch &, SDL_Event const &);
	void _handle_sdl_events();

	void _update_sdl_screen_from_capture()
	{
		Affected_rects const affected = _capture.capture_at(Capture::Point(0, 0));

		_sdl_screen->with_surface([&] (Surface<Pixel> &surface) {

			_captured_screen->with_texture([&] (Texture<Pixel> const &texture) {

				affected.for_each_rect([&] (Capture::Rect const rect) {

					surface.clip(rect);

					Blit_painter::paint(surface, texture, Capture::Point(0, 0));
				});
			});
		});

		/* flush pixels in SDL window */
		affected.for_each_rect([&] (Capture::Rect const rect) {
			_sdl_screen->flush(rect); });
	}

	void _handle_timer()
	{
		_handle_sdl_events();

		_update_sdl_screen_from_capture();
	}

	void _resize(Area size)
	{

		_sdl_screen.construct(size);
		_captured_screen.construct(_capture, _env.rm(), size);
		_update_sdl_screen_from_capture();
	}

	Main(Env &env) : _env(env)
	{
		_resize(Area(_config.xml().attribute_value("width", 1024U),
		             _config.xml().attribute_value("height", 768U)));

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(100000000 / 5994); /* 59.94Hz */
	}
};


void Fb_sdl::Main::_handle_sdl_event(Event_batch &batch, SDL_Event const &event)
{
	using namespace Input;

	if (event.type == SDL_VIDEORESIZE) {

		if (event.resize.w < 0 || event.resize.h < 0) {
			warning("attempt to resize to negative size");
			return;
		}

		_resize(Area((unsigned)event.resize.w, (unsigned)event.resize.h));
		return;
	}

	/* query new mouse position */
	if (event.type == SDL_MOUSEMOTION) {
		int ox = _mx, oy = _my;
		SDL_GetMouseState(&_mx, &_my);

		/* drop superficial events */
		if (ox == _mx && oy == _my)
			return;

		batch.submit(Absolute_motion{_mx, _my});
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

		batch.submit(Release{keycode});
		return;

	case SDL_KEYDOWN:
	case SDL_MOUSEBUTTONDOWN:

		if (event.button.button == SDL_BUTTON_WHEELUP)
			batch.submit(Wheel{0, 1});
		else if (event.button.button == SDL_BUTTON_WHEELDOWN)
			batch.submit(Wheel{0, -1});
		else
			batch.submit(Press{keycode});
		return;

	default:
		break;
	}
}


void Fb_sdl::Main::_handle_sdl_events()
{
	SDL_Event event { };

	_event.with_batch([&] (Event_batch &batch) {

		while (SDL_PollEvent(&event))
			_handle_sdl_event(batch, event);
	});
}


void Component::construct(Genode::Env &env) { static Fb_sdl::Main inst(env); }
