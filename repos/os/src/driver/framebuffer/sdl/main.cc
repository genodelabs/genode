/*
 * \brief  SDL-based implementation of the Genode framebuffer
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2024 Genode Labs GmbH
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
	class Sdl;

	using namespace Genode;

	using Area           = Capture::Area;
	using Rect           = Capture::Rect;
	using Pixel          = Capture::Pixel;
	using Affected_rects = Capture::Session::Affected_rects;
	using Event_batch    = Event::Session_client::Batch;

	static constexpr int USER_EVENT_CAPTURE_WAKEUP = 99;
}


/**
 * Interplay with libSDL
 */
struct Fb_sdl::Sdl : Noncopyable
{
	Event::Connection   &_event;
	Capture::Connection &_capture;
	Region_map          &_rm;

	struct Ticks { Uint32 ms; };

	struct Attr
	{
		Area initial_size;

		double   fps;  /* frames per second */
		unsigned idle; /* disable capturing after 'idle' frames of no progress */

		static Attr from_xml(Xml_node const &node)
		{
			return {
				.initial_size = { .w = node.attribute_value("width",  1024u),
				                  .h = node.attribute_value("height",  768u) },
				.fps  = node.attribute_value("fps", 60.0),
				.idle = node.attribute_value("idle", 3U)
			};
		}

		Ticks period() const
		{
			return { (fps > 0) ? unsigned(1000.0/fps) : 20u };
		}
	};

	Attr const _attr;

	/* fatal exceptions */
	struct Init_failed               : Exception { };
	struct Createthread_failed       : Exception { };
	struct Videodriver_not_supported : Exception { };
	struct Createwindow_failed       : Exception { };
	struct Createrenderer_failed     : Exception { };
	struct Creatergbsurface_failed   : Exception { };
	struct Createtexture_failed      : Exception { };

	void _thread();

	static int _entry(void *data_ptr)
	{
		((Sdl *)data_ptr)->_thread();
		return 0;
	}

	SDL_Thread &_init_thread()
	{
		SDL_Thread *ptr = SDL_CreateThread(_entry, "SDL", this);
		if (ptr)
			return *ptr;

		throw Createthread_failed();
	}

	SDL_Thread &_sdl_thread = _init_thread();

	struct Window
	{
		Area const _initial_size;

		SDL_Renderer &renderer = _init_renderer();

		SDL_Renderer &_init_renderer()
		{
			unsigned const window_flags = 0;

			SDL_Window * const window_ptr =
				SDL_CreateWindow("fb_sdl", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				                 _initial_size.w, _initial_size.h, window_flags);
			if (!window_ptr) {
				error("SDL_CreateWindow failed (", Cstring(SDL_GetError()), ")");
				throw Createwindow_failed();
			}

			SDL_SetWindowResizable(window_ptr, SDL_TRUE);

			int const index = -1;
			unsigned const renderer_flags = SDL_RENDERER_SOFTWARE;
			SDL_Renderer *renderer_ptr = SDL_CreateRenderer(window_ptr, index, renderer_flags);
			if (!renderer_ptr) {
				error("SDL_CreateRenderer failed (", Cstring(SDL_GetError()), ")");
				throw Createrenderer_failed();
			}

			return *renderer_ptr;
		}

		Window(Area size) : _initial_size(size) { }

		~Window()
		{
			SDL_DestroyRenderer(&renderer);
		}
	};

	struct Screen
	{
		Area const size;
		SDL_Renderer &renderer;

		SDL_Surface &_surface = _init_surface();
		SDL_Texture &_texture = _init_texture();

		SDL_Surface &_init_surface()
		{
			unsigned const flags      = 0;
			unsigned const bpp        = 32;
			unsigned const red_mask   = 0x00FF0000;
			unsigned const green_mask = 0x0000FF00;
			unsigned const blue_mask  = 0x000000FF;
			unsigned const alpha_mask = 0xFF000000;

			SDL_Surface * const surface_ptr =
				SDL_CreateRGBSurface(flags, size.w, size.h, bpp,
				                     red_mask, green_mask, blue_mask, alpha_mask);
			if (!surface_ptr) {
				error("SDL_CreateRGBSurface failed (", Cstring(SDL_GetError()), ")");
				throw Creatergbsurface_failed();
			}

			return *surface_ptr;
		}

		SDL_Texture &_init_texture()
		{
			SDL_Texture * const texture_ptr =
				SDL_CreateTexture(&renderer, SDL_PIXELFORMAT_ARGB8888,
				                  SDL_TEXTUREACCESS_STREAMING, size.w, size.h);
			if (!texture_ptr) {
				error("SDL_CreateTexture failed (", Cstring(SDL_GetError()), ")");
				throw Createtexture_failed();
			}

			return *texture_ptr;
		}

		Screen(Area size, SDL_Renderer &renderer) : size(size), renderer(renderer) { }

		~Screen()
		{
			SDL_FreeSurface(&_surface);
			SDL_DestroyTexture(&_texture);
		}

		void with_surface(auto const &fn)
		{
			Surface<Pixel> surface { (Pixel *)_surface.pixels, size };
			fn(surface);
		}

		void flush(Capture::Rect const bounding_box)
		{
			SDL_Rect const rect { .x = bounding_box.at.x,
			                      .y = bounding_box.at.y,
			                      .w = int(bounding_box.area.w),
			                      .h = int(bounding_box.area.h) };

			SDL_UpdateTexture(&_texture, nullptr, _surface.pixels, _surface.pitch);
			SDL_RenderCopy(&renderer, &_texture, &rect, &rect);
			SDL_RenderPresent(&renderer);
		}

		void flush_all() { flush(Rect { { 0, 0 }, size }); }
	};

	Constructible<Window> _window { };
	Constructible<Screen> _screen { };

	Constructible<Capture::Connection::Screen> _captured_screen { };

	int _mx = 0, _my = 0;

	unsigned _capture_woken_up = 0;

	struct Previous_frame
	{
		Ticks timestamp;
		Ticks remaining;    /* remaining ticks to next frame */
		unsigned idle;      /* capture attempts without progress */

		Ticks age() const { return { SDL_GetTicks() - timestamp.ms }; }
	};

	/* if constructed, the processing of a next frame is scheduled */
	Constructible<Previous_frame> _previous_frame { };

	void _schedule_next_frame()
	{
		_previous_frame.construct(
			Previous_frame { .timestamp = { SDL_GetTicks() },
			                 .remaining = { _attr.period() },
			                 .idle      = { } });
	}

	void _handle_event(Event_batch &, SDL_Event const &);

	bool _update_screen_from_capture()
	{
		bool progress = false;
		_screen->with_surface([&] (Surface<Pixel> &surface) {

			Rect const bounding_box = _captured_screen->apply_to_surface(surface);

			progress = (bounding_box.area.count() > 0);

			if (progress)
				_screen->flush(bounding_box);
		});
		return progress;
	}

	void _resize(Area const size)
	{
		_screen.construct(size, _window->renderer);

		using Attr = Capture::Connection::Screen::Attr;
		_captured_screen.construct(_capture, _rm, Attr {
			.px = size,
			.mm = { } });

		_update_screen_from_capture();
		_schedule_next_frame();
	}

	/*
	 * Construction executed by the main thread
	 */
	Sdl(Event::Connection &event, Capture::Connection &capture, Region_map &rm,
	    Attr const attr)
	:
		_event(event), _capture(capture), _rm(rm), _attr(attr)
	{ }
};


void Fb_sdl::Sdl::_thread()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		error("SDL_Init failed (", Cstring(SDL_GetError()), ")");
		throw Init_failed();
	}

	SDL_ShowCursor(0);

	_window.construct(_attr.initial_size);
	_resize(_attr.initial_size);

	/* mainloop */
	for (;;) {

		if (_previous_frame.constructed())
			SDL_WaitEventTimeout(nullptr, _previous_frame->remaining.ms);
		else
			SDL_WaitEvent(nullptr);

		unsigned const orig_capture_woken_up = _capture_woken_up;

		_event.with_batch([&] (Event_batch &batch) {
			SDL_Event event { };
			while (SDL_PollEvent(&event))
				_handle_event(batch, event); });

		Ticks const period = _attr.period();

		bool const woken_up      = (_capture_woken_up != orig_capture_woken_up);
		bool const frame_elapsed = _previous_frame.constructed()
		                        && _previous_frame->age().ms >= period.ms;

		if (woken_up || frame_elapsed) {

			bool const progress = _update_screen_from_capture();
			bool const idle     = !progress && !woken_up;

			if (idle) {
				if (_previous_frame.constructed()) {
					_previous_frame->idle++;
					if (_previous_frame->idle > _attr.idle) {
						_previous_frame.destruct();
						_capture.capture_stopped();
					}
				}
			} else {
				_schedule_next_frame();
			}

		} else {
			/*
			 * Events occurred in-between two frames.
			 * Update timeout for next call of 'SDL_WaitEventTimeout'.
			 */
			if (_previous_frame.constructed())
				_previous_frame->remaining = {
					min(period.ms, period.ms - _previous_frame->age().ms) };
		}
	}
}


void Fb_sdl::Sdl::_handle_event(Event_batch &batch, SDL_Event const &event)
{
	using namespace Input;

	if (event.type == SDL_WINDOWEVENT) {

		if (event.window.event == SDL_WINDOWEVENT_RESIZED) {

			int const w = event.window.data1,
			          h = event.window.data2;

			if (w <= 0 || h <= 0) {
				warning("attempt to resize to invalid size");
				return;
			}
			_resize({ unsigned(w), unsigned(h) });
		}

		_screen->flush_all();
		return;
	}

	if (event.type == SDL_USEREVENT) {
		if (event.user.code == USER_EVENT_CAPTURE_WAKEUP)
			_capture_woken_up++;
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

		/* filter key-repeat events */
		if (event.key.repeat)
			return;

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


struct Fb_sdl::Main
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Event::Connection   _event   { _env };
	Capture::Connection _capture { _env };

	void _handle_capture_wakeup()
	{
		SDL_Event ev { };
		ev.user = SDL_UserEvent { .type      = SDL_USEREVENT,
		                          .timestamp = SDL_GetTicks(),
		                          .windowID  = { },
		                          .code      = USER_EVENT_CAPTURE_WAKEUP,
		                          .data1     = { },
		                          .data2     = { } };

		if (SDL_PushEvent(&ev) == 0)
			warning("SDL_PushEvent failed (", Cstring(SDL_GetError()), ")");
	}

	Signal_handler<Main> _capture_wakeup_handler {
		_env.ep(), *this, &Main::_handle_capture_wakeup };

	Sdl _sdl { _event, _capture, _env.rm(), Sdl::Attr::from_xml(_config.xml()) };

	Main(Env &env) : _env(env)
	{
		_capture.wakeup_sigh(_capture_wakeup_handler);
	}
};


void Component::construct(Genode::Env &env) { static Fb_sdl::Main inst(env); }
