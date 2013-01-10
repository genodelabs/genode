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
#include <cap_session/connection.h>
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

				Event_queue *_ev_queue;
				int          _omx, _omy;
				Element     *_element;

			public:

				Content_event_handler(Event_queue *ev_queue, Element *element):
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

		unsigned                        _fb_w, _fb_h;
		Genode::Attached_ram_dataspace  _fb_ds;
		Pixel_rgb565                   *_pixel;
		unsigned char                  *_alpha;
		Texture_rgb565                  _fb_texture;
		Content_event_handler           _ev_handler;

	public:

		Window_content(unsigned fb_w, unsigned fb_h, Event_queue *ev_queue,
		               bool config_alpha)
		:
			_fb_w(fb_w), _fb_h(fb_h),
			_fb_ds(Genode::env()->ram_session(), _fb_w*_fb_h*sizeof(Pixel_rgb565)),
			_pixel(_fb_ds.local_addr<Pixel_rgb565>()),
			_alpha((unsigned char *)Genode::env()->heap()->alloc(_fb_w*_fb_h)),
			_fb_texture(_pixel, _alpha, _fb_w, _fb_h),
			_ev_handler(ev_queue, this)
		{
			_min_w = _fb_w;
			_min_h = _fb_h;

			int alpha_min = config_alpha ? 0 : 255;

			/* init alpha channel */
			for (unsigned y = 0; y < _fb_h; y++)
				for (unsigned x = 0; x < _fb_w; x++) {

					int v = (x * y + (_fb_w*_fb_h)/4) / _fb_w;
					v = v + (x + y)/2;
					int a = v & 0xff;
					if (v & 0x100)
						a = 255 - a;

					a += (dither_matrix[y % dither_size][x % dither_size] - 127) >> 4;

					_alpha[y*_fb_w + x] = Genode::max(alpha_min, Genode::min(a, 255));
				}

			event_handler(&_ev_handler);
		}

		/**
		 * Accessors
		 */
		Genode::Dataspace_capability fb_ds_cap() { return _fb_ds.cap(); }
		unsigned                     fb_w()      { return _fb_w; }
		unsigned                     fb_h()      { return _fb_h; }

		/**
		 * Element interface
		 */
		void draw(Canvas *c, int x, int y) {
			c->draw_texture(&_fb_texture, _x + x, _y + y); }
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
		public:

			Genode::Dataspace_capability dataspace() {
				return _window_content->fb_ds_cap(); }

			void release() { }

			Mode mode() const
			{
				return Mode(_window_content->fb_w(), _window_content->fb_h(),
				            Mode::RGB565);
			}

			void mode_sigh(Genode::Signal_context_capability) { }

			void refresh(int x, int y, int w, int h) {
				window_content()->redraw_area(x, y, w, h); }
	};


	class Root : public Genode::Root_component<Session_component>
	{
		protected:

			Session_component *_create_session(const char *args) {
				PDBG("creating framebuffer session");
				return new (md_alloc()) Session_component(); }

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc)
			: Genode::Root_component<Session_component>(session_ep, md_alloc) { }
	};
}


void init_services(unsigned fb_w, unsigned fb_h, bool config_alpha)
{
	using namespace Genode;

	static Window_content content(fb_w, fb_h, &_ev_queue, config_alpha);
	_window_content = &content;

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "liquid_fb_ep");

	/*
	 * Let the entry point serve the framebuffer and input root interfaces
	 */
	static Framebuffer::Root    fb_root(&ep, env()->heap());
	static       Input::Root input_root(&ep, env()->heap());

	/*
	 * Now, the root interfaces are ready to accept requests.
	 * This is the right time to tell mummy about our services.
	 */
	env()->parent()->announce(ep.manage(&fb_root));
	env()->parent()->announce(ep.manage(&input_root));
}
