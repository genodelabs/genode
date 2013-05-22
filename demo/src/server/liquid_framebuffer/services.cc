/*
 * \brief  Implementation of Framebuffer and Input services
 * \author Norman Feske
 * \date   2006-09-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/semaphore.h>
#include <base/rpc_server.h>
#include <framebuffer_session/framebuffer_session.h>
#include <input/component.h>

#include "canvas_rgb565.h"
#include "services.h"


/*****************
 ** Event queue **
 *****************/

class Event_queue
{
	private:

		enum { QUEUE_SIZE = 1024 };

		Input::Event      _queue[QUEUE_SIZE];
		int               _head;
		int               _tail;
		Genode::Semaphore _sem;

	public:

		/**
		 * Constructor
		 */
		Event_queue(): _head(0), _tail(0)
		{
			memset(_queue, 0, sizeof(_queue));
		}

		void post(Input::Event ev)
		{
			if ((_head + 1)%QUEUE_SIZE != _tail) {
				_queue[_head] = ev;
				_head = (_head + 1)%QUEUE_SIZE;
				_sem.up();
			}
		}

		Input::Event get()
		{
			_sem.down();
			Input::Event dst_ev = _queue[_tail];
			_tail = (_tail + 1)%QUEUE_SIZE;
			return dst_ev;
		}

		int pending() { return _head != _tail; }

} _ev_queue;


/***************************
 ** Input service backend **
 ***************************/

namespace Input {

	void event_handling(bool enable) { }
	bool event_pending() { return _ev_queue.pending(); }
	Event get_event() { return _ev_queue.get(); }

}


class Window_content : public Element
{
	private:

		class Content_event_handler : public Event_handler
		{
			private:

				Event_queue  *_ev_queue;
				int           _omx, _omy;
				Element      *_element;

			public:

				Content_event_handler(Event_queue *ev_queue, Element *element)
				:
					_ev_queue(ev_queue), _element(element) { }

				void handle(Event &ev)
				{
					int mx = ev.mx - _element->abs_x();
					int my = ev.my - _element->abs_y();

					int code = 0;

					if (ev.type == Event::PRESS || ev.type == Event::RELEASE)
						code = ev.code;

					Input::Event::Type type;

					type = (ev.type == Event::MOTION)  ? Input::Event::MOTION
					     : (ev.type == Event::PRESS)   ? Input::Event::PRESS
					     : (ev.type == Event::RELEASE) ? Input::Event::RELEASE
					     : Input::Event::INVALID;

					if (type != Input::Event::INVALID)
						_ev_queue->post(Input::Event(type, code, mx, my, mx - _omx, my - _omy));

					_omx = mx;
					_omy = my;
				}
		};

		struct Fb_texture
		{
			unsigned                        w, h;
			Genode::Attached_ram_dataspace  ds;
			Pixel_rgb565                   *pixel;
			unsigned char                  *alpha;
			Texture_rgb565                  texture;

			Fb_texture(unsigned w, unsigned h, bool config_alpha)
			:
				w(w), h(h),
				ds(Genode::env()->ram_session(), w*h*sizeof(Pixel_rgb565)),
				pixel(ds.local_addr<Pixel_rgb565>()),
				alpha((unsigned char *)Genode::env()->heap()->alloc(w*h)),
				texture(pixel, alpha, w, h)
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

						a += (dither_matrix[y % dither_size][x % dither_size] - 127) >> 4;

						alpha[y*w + x] = Genode::max(alpha_min, Genode::min(a, 255));
					}
			}

			~Fb_texture()
			{
				Genode::env()->heap()->free(alpha, w*h);
			}

		};

		bool                              _config_alpha;
		Content_event_handler             _ev_handler;
		Fb_texture                       *_fb;
		unsigned                          _new_w, _new_h;
		Genode::Signal_context_capability _mode_sigh;
		bool                              _wait_for_refresh;

	public:

		Window_content(unsigned fb_w, unsigned fb_h, Event_queue *ev_queue,
		               bool config_alpha)
		:
			_config_alpha(config_alpha),
			_ev_handler(ev_queue, this),
			_fb(new (Genode::env()->heap()) Fb_texture(fb_w, fb_h, _config_alpha)),
			_new_w(fb_w), _new_h(fb_h),
			_wait_for_refresh(false)
		{
			_min_w = _fb->w;
			_min_h = _fb->h;

			event_handler(&_ev_handler);
		}

		Genode::Dataspace_capability fb_ds_cap() {
			return _fb->ds.cap();
		}

		unsigned fb_w() {
			return _fb->w;
		}
		unsigned fb_h() {
			return _fb->h;
		}

		void mode_sigh(Genode::Signal_context_capability sigh)
		{
			_mode_sigh = sigh;
		}

		void realloc_framebuffer()
		{
			/* skip reallocation if size has not changed */
			if (_new_w == _fb->w && _new_h == _fb->h)
				return;

			Genode::destroy(Genode::env()->heap(), _fb);

			_fb = new (Genode::env()->heap())
			      Fb_texture(_new_w, _new_h, _config_alpha);

			/*
			 * Suppress drawing of the texture until we received the next
			 * refresh call from the client. This way, we avoid flickering
			 * artifacts while continuously resizing the window.
			 */
			_wait_for_refresh = true;
		}

		void client_called_refresh() { _wait_for_refresh = false; }

		/**
		 * Element interface
		 */
		void draw(Canvas *c, int x, int y)
		{
			if (!_wait_for_refresh)
				c->draw_texture(&_fb->texture, _x + x, _y + y);
		}

		void format_fixed_size(int w, int h)
		{
			_new_w = w, _new_h = h;

			/* notify framebuffer client about mode change */
			if (_mode_sigh.valid())
				Genode::Signal_transmitter(_mode_sigh).submit();
		}
};


static Window_content *_window_content;

Element *window_content() { return _window_content; }


/***********************************************
 ** Implementation of the framebuffer service **
 ***********************************************/

namespace Framebuffer
{
	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Window_content &_window_content;

		public:

			Session_component(Window_content &window_content)
			: _window_content(window_content) { }

			Genode::Dataspace_capability dataspace() {
				return _window_content.fb_ds_cap(); }

			void release() {
				_window_content.realloc_framebuffer(); }

			Mode mode() const
			{
				return Mode(_window_content.fb_w(), _window_content.fb_h(),
				            Mode::RGB565);
			}

			void mode_sigh(Genode::Signal_context_capability sigh) {
				_window_content.mode_sigh(sigh); }

			void refresh(int x, int y, int w, int h)
			{
				_window_content.client_called_refresh();
				_window_content.redraw_area(x, y, w, h);
			}
	};


	class Root : public Genode::Root_component<Session_component>
	{
		private:

			Window_content &_window_content;

		protected:

			Session_component *_create_session(const char *args) {
				return new (md_alloc()) Session_component(_window_content); }

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc,
			     Window_content         &window_content)
			:
				Genode::Root_component<Session_component>(session_ep, md_alloc),
				_window_content(window_content)
			{ }
	};
}


void init_window_content(unsigned fb_w, unsigned fb_h, bool config_alpha)
{
	static Window_content content(fb_w, fb_h, &_ev_queue, config_alpha);
	_window_content = &content;
}


void init_services(Genode::Rpc_entrypoint &ep)
{
	using namespace Genode;

	/*
	 * Let the entry point serve the framebuffer and input root interfaces
	 */
	static Framebuffer::Root    fb_root(&ep, env()->heap(), *_window_content);
	static       Input::Root input_root(&ep, env()->heap());

	/*
	 * Now, the root interfaces are ready to accept requests.
	 * This is the right time to tell mummy about our services.
	 */
	env()->parent()->announce(ep.manage(&fb_root));
	env()->parent()->announce(ep.manage(&input_root));
}
