/*
 * \brief  Implementation of Framebuffer and Input services
 * \author Norman Feske
 * \date   2006-09-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode include */
#include <base/env.h>
#include <base/semaphore.h>
#include <base/rpc_server.h>
#include <framebuffer_session/framebuffer_session.h>
#include <input/root.h>
#include <nitpicker_gfx/texture_painter.h>
#include <os/pixel_rgb888.h>
#include <os/static_root.h>
#include <timer_session/connection.h>

/* local includes */
#include "services.h"


typedef Genode::Texture<Genode::Pixel_rgb888> Texture_rgb888;


class Window_content : public Scout::Element
{
	private:

		class Content_event_handler : public Scout::Event_handler
		{
			private:

				/*
				 * Noncopyable
				 */
				Content_event_handler(Content_event_handler const &);
				Content_event_handler &operator = (Content_event_handler const &);

				Input::Session_component &_input_session;
				Scout::Point              _old_mouse_position { };
				Element                  *_element;

			public:

				Content_event_handler(Input::Session_component &input_session,
				                      Scout::Element *element)
				:
					_input_session(input_session), _element(element)
				{ }

				void handle_event(Scout::Event const &ev) override
				{
					using namespace Scout;

					Point mouse_position = ev.mouse_position - _element->abs_position();

					auto motion = [&] (Point p) { return Input::Absolute_motion{p.x(), p.y()}; };

					if (ev.type == Event::MOTION)
						_input_session.submit(motion(mouse_position));

					if (ev.type == Event::PRESS)
						_input_session.submit(Input::Press{Input::Keycode(ev.code)});

					if (ev.type == Event::RELEASE)
						_input_session.submit(Input::Release{Input::Keycode(ev.code)});

					_old_mouse_position = mouse_position;
				}
		};

		struct Fb_texture
		{
			Genode::Allocator                    &alloc;
			unsigned                              w, h;
			Genode::Attached_ram_dataspace        ds;
			Genode::Pixel_rgb888                 *pixel;
			unsigned char                        *alpha;
			Genode::Texture<Genode::Pixel_rgb888> texture;

			Fb_texture(Genode::Ram_allocator &ram, Genode::Region_map &local_rm,
			           Genode::Allocator &alloc,
			           unsigned w, unsigned h, bool config_alpha)
			:
				alloc(alloc), w(w), h(h),
				ds(ram, local_rm, w*h*sizeof(Genode::Pixel_rgb888)),
				pixel(ds.local_addr<Genode::Pixel_rgb888>()),
				alpha((unsigned char *)alloc.alloc(w*h)),
				texture(pixel, alpha, Scout::Area(w, h))
			{
				int alpha_min = config_alpha ? 0 : 255;

				/* init alpha channel */
				for (unsigned y = 0; y < h; y++)
					for (unsigned x = 0; x < w; x++) {

						int v = (x * y + (w*h)/4) / w;
						v = v + (x + y)/2;
						int a = v & 0xff;
						if (v & 0x100)
							a = 255 - a;

						a += (Genode::Dither_matrix::value(x, y) - 127) >> 4;

						alpha[y*w + x] = (unsigned char)Genode::max(alpha_min, Genode::min(a, 255));
					}
			}

			~Fb_texture()
			{
				alloc.free(alpha, w*h);
			}

			private:

				/*
				 * Noncopyable
				 */
				Fb_texture(Fb_texture const &);
				Fb_texture &operator = (Fb_texture const &);

		};

		Genode::Ram_allocator &_ram;
		Genode::Region_map    &_rm;
		Genode::Allocator     &_alloc;
		bool                   _config_alpha;
		Content_event_handler  _ev_handler;

		Genode::Reconstructible<Fb_texture> _fb;

		/**
		 * Size of the framebuffer handed out by the next call of 'dataspace'
		 */
		Scout::Area _next_size;

		/**
		 * Most current designated size of the framebuffer as defined by the
		 * user.
		 *
		 * The '_designated_size' may be updated any time when the user drags
		 * the resize handle of the window. It is propagated to '_next_size'
		 * not before the framebuffer client requests the current mode. Once
		 * the mode information is passed to the client, it is locked until
		 * the client requests the mode again.
		 */
		Scout::Area _designated_size;

		Genode::Signal_context_capability _mode_sigh { };

	public:

		Window_content(Genode::Ram_allocator &ram, Genode::Region_map &rm,
		               Genode::Allocator &alloc, unsigned fb_w, unsigned fb_h,
		               Input::Session_component &input_session,
		               bool config_alpha)
		:
			_ram(ram), _rm(rm), _alloc(alloc),
			_config_alpha(config_alpha),
			_ev_handler(input_session, this),
			_fb(_ram, _rm, _alloc, fb_w, fb_h, _config_alpha),
			_next_size(fb_w, fb_h),
			_designated_size(_next_size)
		{
			_min_size = Scout::Area(_fb->w, _fb->h);

			event_handler(&_ev_handler);
		}

		Genode::Dataspace_capability fb_ds_cap() { return _fb->ds.cap(); }

		unsigned fb_w() { return _fb->w; }
		unsigned fb_h() { return _fb->h; }

		Scout::Area mode_size()
		{
			_next_size = _designated_size;
			return _next_size;
		}

		void mode_sigh(Genode::Signal_context_capability sigh)
		{
			_mode_sigh = sigh;
		}

		void realloc_framebuffer()
		{
			/* skip reallocation if size has not changed */
			if (_next_size.w() == _fb->w && _next_size.h() == _fb->h)
				return;

			_fb.construct(_ram, _rm, _alloc, _next_size.w(), _next_size.h(), _config_alpha);
		}

		/**
		 * Element interface
		 */
		void draw(Scout::Canvas_base &canvas, Scout::Point abs_position) override
		{
			canvas.draw_texture(abs_position + _position, _fb->texture);
		}

		void format_fixed_size(Scout::Area size) override
		{
			_designated_size = size;

			/* notify framebuffer client about mode change */
			if (_mode_sigh.valid())
				Genode::Signal_transmitter(_mode_sigh).submit();
		}
};


static Window_content *_window_content;

Scout::Element *window_content() { return _window_content; }


/***********************************************
 ** Implementation of the framebuffer service **
 ***********************************************/

namespace Framebuffer { class Session_component; }

class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		Timer::Connection _timer;

		Window_content &_window_content;

	public:

		Session_component(Genode::Env &env, Window_content &window_content)
		: _timer(env), _window_content(window_content) { }

		Genode::Dataspace_capability dataspace() override
		{
			_window_content.realloc_framebuffer();
			return _window_content.fb_ds_cap();
		}

		Mode mode() const override
		{
			return Mode { .area = _window_content.mode_size() };
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override {
			_window_content.mode_sigh(sigh); }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override
		{
			_window_content.redraw_area(x, y, w, h);
		}
};


void init_window_content(Genode::Ram_allocator &ram, Genode::Region_map &rm,
                         Genode::Allocator &alloc,
                         Input::Session_component &input_component,
                         unsigned fb_w, unsigned fb_h, bool config_alpha)
{
	static Window_content content(ram, rm, alloc, fb_w, fb_h,
	                              input_component, config_alpha);
	_window_content = &content;
}


void init_services(Genode::Env &env, Input::Session_component &input_component)
{
	using namespace Genode;

	static Framebuffer::Session_component fb_session(env, *_window_content);
	static Static_root<Framebuffer::Session> fb_root(env.ep().manage(fb_session));

	static Input::Root_component input_root(env.ep().rpc_ep(), input_component);

	/*
	 * Now, the root interfaces are ready to accept requests.
	 * This is the right time to tell mummy about our services.
	 */
	env.parent().announce(env.ep().manage(fb_root));
	env.parent().announce(env.ep().manage(input_root));
}
