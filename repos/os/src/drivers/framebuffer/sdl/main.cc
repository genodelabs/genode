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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wtype-limits"
#include <SDL2/SDL.h>
#pragma GCC diagnostic pop

/* local includes */
#include "convert_keycode.h"

namespace Fb_sdl {
	class Main;
	using namespace Genode;
}


/* fatal exceptions */
struct Sdl_init_failed               : Genode::Exception { };
struct Sdl_videodriver_not_supported : Genode::Exception { };
struct Sdl_createwindow_failed       : Genode::Exception { };
struct Sdl_createrenderer_failed     : Genode::Exception { };
struct Sdl_creatergbsurface_failed   : Genode::Exception { };
struct Sdl_createtexture_failed      : Genode::Exception { };


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

		SDL_ShowCursor(0);
	}

	bool const _sdl_initialized = ( _init_sdl(), true );

	struct Sdl_screen
	{
		Area const size;

		SDL_Renderer &_sdl_renderer = _init_sdl_renderer();
		SDL_Surface &_sdl_surface = _init_sdl_surface();
		SDL_Texture &_sdl_texture = _init_sdl_texture();

		SDL_Renderer &_init_sdl_renderer()
		{
			unsigned const window_flags = 0;

			SDL_Window *window_ptr = SDL_CreateWindow("fb_sdl", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, size.w(), size.h(), window_flags);
			if (!window_ptr) {
				error("SDL_CreateWindow failed (", Genode::Cstring(SDL_GetError()), ")");
				throw Sdl_createwindow_failed();
			}

			SDL_SetWindowResizable(window_ptr, SDL_TRUE);

			int const index = -1;
			unsigned const renderer_flags = SDL_RENDERER_SOFTWARE;
			SDL_Renderer *renderer_ptr = SDL_CreateRenderer(window_ptr, index, renderer_flags);
			if (!renderer_ptr) {
				error("SDL_CreateRenderer failed (", Genode::Cstring(SDL_GetError()), ")");
				throw Sdl_createrenderer_failed();
			}

			return *renderer_ptr;
		}

		SDL_Surface &_init_sdl_surface()
		{
			unsigned const flags      = 0;
			unsigned const bpp        = 32;
			unsigned const red_mask   = 0x00FF0000;
			unsigned const green_mask = 0x0000FF00;
			unsigned const blue_mask  = 0x000000FF;
			unsigned const alpha_mask = 0xFF000000;

			SDL_Surface *surface_ptr = SDL_CreateRGBSurface(flags, size.w(), size.h(), bpp, red_mask, green_mask, blue_mask, alpha_mask);
			if (!surface_ptr) {
				error("SDL_CreateRGBSurface failed (", Genode::Cstring(SDL_GetError()), ")");
				throw Sdl_creatergbsurface_failed();
			}

			return *surface_ptr;
		}

		SDL_Texture &_init_sdl_texture()
		{
			SDL_Texture *texture_ptr = SDL_CreateTexture(&_sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, size.w(), size.h());
			if (!texture_ptr) {
				error("SDL_CreateTexture failed (", Genode::Cstring(SDL_GetError()), ")");
				throw Sdl_createtexture_failed();
			}

			return *texture_ptr;
		}

		Sdl_screen(Area size) : size(size) { }

		template <typename FN>
		void with_surface(FN const &fn)
		{
			Surface<Pixel> surface { (Pixel *)_sdl_surface.pixels, size };
			fn(surface);
		}

		void flush()
		{
			SDL_UpdateTexture(&_sdl_texture, nullptr, _sdl_surface.pixels, _sdl_surface.pitch);
			SDL_RenderClear(&_sdl_renderer);
			SDL_RenderCopy(&_sdl_renderer, &_sdl_texture, nullptr, nullptr);
			SDL_RenderPresent(&_sdl_renderer);
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
		_sdl_screen->flush();
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

	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {

		int w = event.window.data1;
		int h = event.window.data2;
		if (w < 0 || h < 0) {
			warning("attempt to resize to negative size");
			return;
		}

		_resize(Area((unsigned)w, (unsigned)h));
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

		batch.submit(Release{keycode});
		return;

	case SDL_KEYDOWN:
	case SDL_MOUSEBUTTONDOWN:

		batch.submit(Press{keycode});
		return;

	case SDL_MOUSEWHEEL:

		if (event.wheel.y > 0)
			batch.submit(Wheel{0, 1});
		else if (event.wheel.y < 0)
			batch.submit(Wheel{0, -1});
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
