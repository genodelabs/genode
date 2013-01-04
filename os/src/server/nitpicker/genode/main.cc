/*
 * \brief  Nitpicker main program for Genode
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <os/attached_ram_dataspace.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <root/component.h>
#include <dataspace/client.h>
#include <cap_session/connection.h>
#include <timer_session/connection.h>
#include <input_session/connection.h>
#include <input_session/input_session.h>
#include <nitpicker_view/nitpicker_view.h>
#include <nitpicker_session/nitpicker_session.h>
#include <framebuffer_session/connection.h>
#include <nitpicker_gfx/pixel_rgb565.h>
#include <nitpicker_gfx/string.h>
#include <os/session_policy.h>

/* local includes */
#include "big_mouse.h"
#include "background.h"
#include "user_state.h"
#include "clip_guard.h"
#include "mouse_cursor.h"
#include "chunky_menubar.h"


/***************
 ** Utilities **
 ***************/

/**
 * Determine session color according to the list of configured policies
 *
 * Select the policy that matches the label. If multiple policies
 * match, select the one with the largest number of characters.
 */
static Color session_color(char const *session_args)
{
	/* use white by default */
	Color color = WHITE;

	try {
		Genode::Session_policy policy(session_args);

		/* read color attribute */
		char color_buf[8];
		policy.attribute("color").value(color_buf, sizeof(color_buf));
		Genode::ascii_to(color_buf, &color);
	} catch (...) { }

	return color;
}


/*
 * Font initialization
 */
extern char _binary_default_tff_start;

Font default_font(&_binary_default_tff_start);


class Flush_merger
{
	private:

		Rect _to_be_flushed;

	public:

		bool defer;

		Flush_merger() : _to_be_flushed(Point(), Area(-1, -1)), defer(false) { }

		Rect to_be_flushed() { return _to_be_flushed; }

		void merge(Rect rect)
		{
			if (_to_be_flushed.valid())
				_to_be_flushed = Rect::compound(_to_be_flushed, rect);
			else
				_to_be_flushed = rect;
		}

		void reset() { _to_be_flushed = Rect(Point(), Area(-1, -1)); }
};


template <typename PT>
class Screen : public Chunky_canvas<PT>, public Flush_merger
{
	protected:

		virtual void _flush_pixels(Rect rect) { merge(rect); }

	public:

		/**
		 * Constructor
		 */
		Screen(PT *scr_base, Area scr_size):
			Chunky_canvas<PT>(scr_base, scr_size) { }
};


class Buffer
{
	private:

		Area                           _size;
		Framebuffer::Mode::Format      _format;
		Genode::Attached_ram_dataspace _ram_ds;

	public:

		/**
		 * Constructor - allocate and map dataspace for virtual frame buffer
		 *
		 * \throw Ram_session::Alloc_failed
		 * \throw Rm_session::Attach_failed
		 */
		Buffer(Area size, Framebuffer::Mode::Format format, Genode::size_t bytes):
			_size(size), _format(format),
			_ram_ds(Genode::env()->ram_session(), bytes)
		{ }

		/**
		 * Accessors
		 */
		Genode::Ram_dataspace_capability ds_cap() { return _ram_ds.cap(); }
		Area                               size() { return _size; }
		Framebuffer::Mode::Format        format() { return _format; }
		void                        *local_addr() { return _ram_ds.local_addr<void>(); }
};


template <typename PT>
class Chunky_dataspace_texture : public Buffer, public Chunky_texture<PT>
{
	private:

		Framebuffer::Mode::Format _format() {
			return Framebuffer::Mode::RGB565; }

		/**
		 * Return base address of alpha channel or 0 if no alpha channel exists
		 */
		unsigned char *_alpha_base(Area size, bool use_alpha)
		{
			if (!use_alpha) return 0;

			/* alpha values come right after the pixel values */
			return (unsigned char *)local_addr() + calc_num_bytes(size, false);
		}

	public:

		/**
		 * Constructor
		 */
		Chunky_dataspace_texture(Area size, bool use_alpha):
			Buffer(size, _format(), calc_num_bytes(size, use_alpha)),
			Chunky_texture<PT>((PT *)local_addr(),
			                   _alpha_base(size, use_alpha), size) { }

		static Genode::size_t calc_num_bytes(Area size, bool use_alpha)
		{
			/*
			 * If using an alpha channel, the alpha buffer follows the
			 * pixel buffer. The alpha buffer is followed by an input
			 * mask buffer. Hence, we have to account one byte per
			 * alpha value and one byte for the input mask value.
			 */
			Genode::size_t bytes_per_pixel = sizeof(PT) + (use_alpha ? 2 : 0);
			return bytes_per_pixel*size.w()*size.h();
		}

		unsigned char *input_mask_buffer()
		{
			if (!Chunky_texture<PT>::alpha()) return 0;

			/* input-mask values come right after the alpha values */
			return (unsigned char *)local_addr() + calc_num_bytes(*this, false)
			                                     + Texture::w()*Texture::h();
		}
};


/***********************
 ** Input sub session **
 ***********************/

namespace Input {

	class Session_component : public Genode::Rpc_object<Session>
	{
		public:

			enum { MAX_EVENTS = 200 };

		private:

			/*
			 * Exported event buffer dataspace
			 */
			Genode::Attached_ram_dataspace _ev_ram_ds;

			/*
			 * Local event buffer that is copied
			 * to the exported event buffer when
			 * flush() gets called.
			 */
			Event      _ev_buf[MAX_EVENTS];
			unsigned   _num_ev;

		public:

			static Genode::size_t ev_ds_size() {
				return Genode::align_addr(MAX_EVENTS*sizeof(Event), 12); }

			/**
			 * Constructor
			 */
			Session_component():
				_ev_ram_ds(Genode::env()->ram_session(), ev_ds_size()),
				_num_ev(0) { }

			/**
			 * Enqueue event into local event buffer of the input session
			 */
			void submit(const Event *ev)
			{
				/* drop event when event buffer is full */
				if (_num_ev >= MAX_EVENTS) return;

				/* insert event into local event buffer */
				_ev_buf[_num_ev++] = *ev;
			}


			/*****************************
			 ** Input session interface **
			 *****************************/

			Genode::Dataspace_capability dataspace() { return _ev_ram_ds.cap(); }

			bool is_pending() const { return _num_ev > 0; }

			int flush()
			{
				unsigned ev_cnt;

				/* copy events from local event buffer to exported buffer */
				Event *ev_ds_buf = _ev_ram_ds.local_addr<Event>();
				for (ev_cnt = 0; ev_cnt < _num_ev; ev_cnt++)
					ev_ds_buf[ev_cnt] = _ev_buf[ev_cnt];

				_num_ev = 0;
				return ev_cnt;
			}
	};
}


/*****************************
 ** Framebuffer sub session **
 *****************************/

namespace Framebuffer {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			::Buffer             *_buffer;
			View_stack           *_view_stack;
			::Session            *_session;
			Flush_merger         *_flush_merger;
			Framebuffer::Session *_framebuffer;

		public:

			/**
			 * Constructor
			 *
			 * \param session  Nitpicker session
			 */
			Session_component(::Buffer *buffer, View_stack *view_stack,
			                  ::Session *session, Flush_merger *flush_merger,
			                  Framebuffer::Session *framebuffer)
			:
				_buffer(buffer), _view_stack(view_stack), _session(session),
				_flush_merger(flush_merger), _framebuffer(framebuffer) { }

			Genode::Dataspace_capability dataspace() { return _buffer->ds_cap(); }

			void release() { }

			Mode mode() const
			{
				return Mode(_buffer->size().w(), _buffer->size().h(),
				            _buffer->format());
			}

			void mode_sigh(Genode::Signal_context_capability) { }

			void refresh(int x, int y, int w, int h)
			{
				_view_stack->update_session_views(_session,
				                                  Rect(Point(x, y), Area(w, h)));

				/* flush dirty pixels to physical frame buffer */
				if (_flush_merger->defer == false) {
					Rect r = _flush_merger->to_be_flushed();
					_framebuffer->refresh(r.x1(), r.y1(), r.w(), r.h());
					_flush_merger->reset();
				}
				_flush_merger->defer = true;
			}
	};
}


class View_component : public Genode::List<View_component>::Element,
                       public Genode::Rpc_object<Nitpicker::View>
{
	private:

		View_stack             *_view_stack;
		::View                  _view;
		Genode::Rpc_entrypoint *_ep;

		static unsigned _flags(Session *session)
		{
			return (session && session->stay_top()) ? ::View::STAY_TOP : 0;
		}

	public:

		/**
		 * Constructor
		 */
		View_component(::Session *session, View_stack *view_stack,
		               Genode::Rpc_entrypoint *ep):
			_view_stack(view_stack), _view(session, _flags(session)), _ep(ep) { }

		::View *view() { return &_view; }


		/******************************
		 ** Nitpicker view interface **
		 ******************************/

		int viewport(int x, int y, int w, int h,
		             int buf_x, int buf_y, bool redraw)
		{
			/* transpose y position by vertical session offset */
			y += _view.session()->v_offset();

			_view_stack->viewport(&_view, Rect(Point(x, y), Area(w, h)),
			                              Point(buf_x, buf_y), redraw);
			return 0;
		}

		int stack(Nitpicker::View_capability neighbor_cap, bool behind, bool redraw)
		{
			Genode::Object_pool<View_component>::Guard nvc(_ep->lookup_and_lock(neighbor_cap));

			::View *neighbor_view = nvc ? nvc->view() : 0;

			_view_stack->stack(&_view, neighbor_view, behind, redraw);
			return 0;
		}

		int title(Title const &title)
		{
			_view_stack->title(&_view, title.string());
			return 0;
		}
};


/*****************************************
 ** Implementation of Nitpicker service **
 *****************************************/

namespace Nitpicker {

	class Session_component : public Genode::Rpc_object<Session>, public ::Session
	{
		private:

			/* Framebuffer_session_component */
			Framebuffer::Session_component _framebuffer_session_component;

			/* Input_session_component */
			Input::Session_component  _input_session_component;

			/*
			 * Entrypoint that is used for the views, input session,
			 * and framebuffer session.
			 */
			Genode::Rpc_entrypoint *_ep;

			View_stack *_view_stack;

			Genode::List<View_component> _view_list;

			/* capabilities for sub sessions */
			Framebuffer::Session_capability _framebuffer_session_cap;
			Input::Session_capability       _input_session_cap;

			bool _provides_default_bg;

		public:

			/**
			 * Constructor
			 */
			Session_component(const char             *name,
			                  ::Buffer               *buffer,
			                  Texture                *texture,
			                  View_stack             *view_stack,
			                  Genode::Rpc_entrypoint *ep,
			                  Flush_merger           *flush_merger,
			                  Framebuffer::Session   *framebuffer,
			                  int                     v_offset,
			                  unsigned char          *input_mask,
			                  bool                    provides_default_bg,
			                  Color                   color,
			                  bool                    stay_top)
			:
				::Session(name, texture, v_offset, color, input_mask, stay_top),
				_framebuffer_session_component(buffer, view_stack, this, flush_merger, framebuffer),
				_ep(ep), _view_stack(view_stack),
				_framebuffer_session_cap(_ep->manage(&_framebuffer_session_component)),
				_input_session_cap(_ep->manage(&_input_session_component)),
				_provides_default_bg(provides_default_bg)
			{ }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				_ep->dissolve(&_framebuffer_session_component);
				_ep->dissolve(&_input_session_component);

				View_component *vc;
				while ((vc = _view_list.first()))
					destroy_view(vc->cap());
			}


			/******************************************
			 ** Nitpicker-internal session interface **
			 ******************************************/

			void submit_input_event(Input::Event *ev)
			{
				using namespace Input;

				/*
				 * Transpose absolute coordinates by session-specific vertical
				 * offset.
				 */
				Event e = *ev;
				if (e.ax() || e.ay())
					e = Event(e.type(), e.code(), e.ax(),
					          max(0, e.ay() - v_offset()), e.rx(), e.ry());

				_input_session_component.submit(&e);
			}


			/*********************************
			 ** Nitpicker session interface **
			 *********************************/

			Framebuffer::Session_capability framebuffer_session() {
				return _framebuffer_session_cap; }

			Input::Session_capability input_session() {
				return _input_session_cap; }

			View_capability create_view()
			{
				/**
				 * FIXME: Do not allocate View meta data from Heap!
				 *        Use a heap partition!
				 */
				View_component *view = new (Genode::env()->heap())
				                       View_component(this, _view_stack, _ep);

				_view_list.insert(view);
				return _ep->manage(view);
			}

			void destroy_view(View_capability view_cap)
			{
				View_component *vc = dynamic_cast<View_component *>(_ep->lookup_and_lock(view_cap));
				if (!vc) return;

				_view_stack->remove_view(vc->view());
				_ep->dissolve(vc);
				_view_list.remove(vc);
				destroy(Genode::env()->heap(), vc);
			}

			int background(View_capability view_cap)
			{
				if (_provides_default_bg) {
					Genode::Object_pool<View_component>::Guard vc(_ep->lookup_and_lock(view_cap));
					vc->view()->background(true);
					_view_stack->default_background(vc->view());
					return 0;
				}

				/* revert old background view to normal mode */
				if (::Session::background()) ::Session::background()->background(false);

				/* assign session background */
				Genode::Object_pool<View_component>::Guard vc(_ep->lookup_and_lock(view_cap));
				::Session::background(vc->view());

				/* switch background view to background mode */
				if (::Session::background()) ::Session::background()->background(true);

				return 0;
			}
	};


	template <typename PT>
	class Root : public Genode::Root_component<Session_component>
	{
		private:

			Area                  _scr_size;
			View_stack           *_view_stack;
			Flush_merger         *_flush_merger;
			Framebuffer::Session *_framebuffer;
			int                   _default_v_offset;

		protected:

			Session_component *_create_session(const char *args)
			{
				PINF("create session with args: %s\n", args);
				Genode::size_t ram_quota = Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);

				int v_offset = _default_v_offset;

				/* determine buffer size of the session */
				Area size(Genode::Arg_string::find_arg(args, "fb_width" ).long_value(_scr_size.w()),
				          Genode::Arg_string::find_arg(args, "fb_height").long_value(_scr_size.h() - v_offset));

				char label_buf[::Session::LABEL_LEN];
				Genode::Arg_string::find_arg(args, "label").string(label_buf, sizeof(label_buf), "<unlabeled>");

				bool use_alpha = Genode::Arg_string::find_arg(args, "alpha").bool_value(false);
				bool stay_top  = Genode::Arg_string::find_arg(args, "stay_top").bool_value(false);

				Genode::size_t texture_num_bytes = Chunky_dataspace_texture<PT>::calc_num_bytes(size, use_alpha);

				Genode::size_t required_quota = texture_num_bytes
				                              + Input::Session_component::ev_ds_size();

				if (ram_quota < required_quota) {
					PWRN("Insufficient dontated ram_quota (%zd bytes), require %zd bytes",
					     ram_quota, required_quota);
					throw Genode::Root::Quota_exceeded();
				}

				/* allocate texture */
				Chunky_dataspace_texture<PT> *cdt;
				cdt = new (md_alloc()) Chunky_dataspace_texture<PT>(size, use_alpha);

				bool provides_default_bg = (Genode::strcmp(label_buf, "backdrop") == 0);

				return new (md_alloc())
				       Session_component(label_buf, cdt, cdt, _view_stack, ep(),
				                         _flush_merger, _framebuffer, v_offset,
				                         cdt->input_mask_buffer(),
				                         provides_default_bg, session_color(args),
				                         stay_top);
			}

			void _destroy_session(Session_component *session)
			{
				Chunky_dataspace_texture<PT> *cdt;
				cdt = static_cast<Chunky_dataspace_texture<PT> *>(session->texture());

				destroy(md_alloc(), session);
				destroy(md_alloc(), cdt);
			}

		public:

			/**
			 * Constructor
			 */
			Root(Genode::Rpc_entrypoint *session_ep, Area scr_size,
			     View_stack *view_stack, Genode::Allocator *md_alloc,
			     Flush_merger *flush_merger,
			     Framebuffer::Session *framebuffer, int default_v_offset)
			:
				Genode::Root_component<Session_component>(session_ep, md_alloc),
				_scr_size(scr_size), _view_stack(view_stack), _flush_merger(flush_merger),
				_framebuffer(framebuffer), _default_v_offset(default_v_offset) { }
	};
}


/*******************
 ** Input handler **
 *******************/

struct Input_handler
{
	GENODE_RPC(Rpc_do_input_handling, void, do_input_handling);
	GENODE_RPC_INTERFACE(Rpc_do_input_handling);
};


class Input_handler_component : public Genode::Rpc_object<Input_handler,
                                                          Input_handler_component>
{
	private:

		User_state           *_user_state;
		View                 *_mouse_cursor;
		Area                  _mouse_size;
		Flush_merger         *_flush_merger;
		Input::Session       *_input;
		Input::Event         *_ev_buf;
		Framebuffer::Session *_framebuffer;
		Timer::Session       *_timer;

	public:

		/**
		 * Constructor
		 */
		Input_handler_component(User_state *user_state, View *mouse_cursor,
		                        Area mouse_size, Flush_merger *flush_merger,
		                        Input::Session *input,
		                        Framebuffer::Session *framebuffer,
		                        Timer::Session *timer)
		:
			_user_state(user_state), _mouse_cursor(mouse_cursor),
			_mouse_size(mouse_size), _flush_merger(flush_merger),
			_input(input),
			_ev_buf(Genode::env()->rm_session()->attach(_input->dataspace())),
			_framebuffer(framebuffer), _timer(timer)
		{ }

		/**
		 * Determine number of events that can be merged into one
		 *
		 * \param ev   pointer to first event array element to check
		 * \param max  size of the event array
		 * \return     number of events subjected to merge
		 */
		unsigned _num_consecutive_events(Input::Event *ev, unsigned max)
		{
			if (max < 1) return 0;
			if (ev->type() != Input::Event::MOTION) return 1;

			bool first_is_absolute = ev->is_absolute_motion();

			/* iterate until we get a different event type, start at second */
			unsigned cnt = 1;
			for (ev++ ; cnt < max; cnt++, ev++) {
				if (ev->type() != Input::Event::MOTION) break;
				if (first_is_absolute != ev->is_absolute_motion()) break;
			}
			return cnt;
		}

		/**
		 * Merge consecutive motion events
		 *
		 * \param ev  event array to merge
		 * \param n   number of events to merge
		 * \return    merged motion event
		 */
		Input::Event _merge_motion_events(Input::Event *ev, unsigned n)
		{
			Input::Event res;
			for (unsigned i = 0; i < n; i++, ev++)
				res = Input::Event(Input::Event::MOTION, 0, ev->ax(), ev->ay(),
				                   res.rx() + ev->rx(), res.ry() + ev->ry());
			return res;
		}

		void _import_input_events(unsigned num_ev)
		{
			/*
			 * Take events from input event buffer, merge consecutive motion
			 * events, and pass result to the user state.
			 */
			for (unsigned src_ev_cnt = 0; src_ev_cnt < num_ev; src_ev_cnt++) {

				Input::Event *e = &_ev_buf[src_ev_cnt];
				Input::Event curr = *e;

				if (e->type() == Input::Event::MOTION) {
					unsigned n = _num_consecutive_events(e, num_ev - src_ev_cnt);
					curr = _merge_motion_events(e, n);

					/* skip merged events */
					src_ev_cnt += n - 1;
				}

				/*
				 * If subsequential relative motion events are merged to
				 * a zero-motion event, drop it. Otherwise, it would be
				 * misinterpreted as absolute event pointing to (0, 0).
				 */
				if (e->is_relative_motion() && curr.rx() == 0 && curr.ry() == 0)
					continue;

				/* pass event to user state */
				_user_state->handle_event(curr);
			}
		}

		/**
		 * This function is called periodically from the timer loop
		 */
		void do_input_handling()
		{
			do {
				Point old_mouse_pos = _user_state->mouse_pos();

				/* handle batch of pending events */
				if (_input->is_pending())
					_import_input_events(_input->flush());

				Point new_mouse_pos = _user_state->mouse_pos();

				/* update mouse cursor */
				if (old_mouse_pos != new_mouse_pos)
					_user_state->viewport(_mouse_cursor,
					                      Rect(new_mouse_pos, _mouse_size),
					                      Point(), true);

				/* flush dirty pixels to physical frame buffer */
				if (_flush_merger->defer == false) {
					Rect r = _flush_merger->to_be_flushed();
					if (r.valid())
						_framebuffer->refresh(r.x1(), r.y1(), r.w(), r.h());
					_flush_merger->reset();
				}
				_flush_merger->defer = false;

				/*
				 * In kill mode, we never leave the dispatch function to block
				 * RPC calls from Nitpicker clients. We sleep here to make the
				 * spinning for the end of the kill mode less painful for all
				 * non-blocked processes.
				 */
				if (_user_state->kill())
					_timer->msleep(10);

			} while (_user_state->kill());
		}
};


typedef Pixel_rgb565 PT;  /* physical pixel type */


int main(int argc, char **argv)
{
	using namespace Genode;

	/*
	 * Sessions to the required external services
	 */
	static Timer::Connection       timer;
	static Framebuffer::Connection framebuffer;
	static Input::Connection       input;
	static Cap_connection          cap;

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 16*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nitpicker_ep");

	/*
	 * Initialize framebuffer
	 */
	Framebuffer::Mode const mode = framebuffer.mode();

	PINF("framebuffer is %dx%d@%d\n",
	     mode.width(), mode.height(), mode.format());

	Dataspace_capability fb_ds_cap = framebuffer.dataspace();
	if (!fb_ds_cap.valid()) {
		PERR("Could not request dataspace for frame buffer");
		return -2;
	}

	void *fb_base = env()->rm_session()->attach(fb_ds_cap);
	Screen<PT> screen((PT *)fb_base, Area(mode.width(), mode.height()));

	enum { MENUBAR_HEIGHT = 16 };
	PT *menubar_pixels = (PT *)env()->heap()->alloc(sizeof(PT)*mode.width()*16);
	Chunky_menubar<PT> menubar(menubar_pixels, Area(mode.width(), MENUBAR_HEIGHT));

	User_state user_state(&screen, &menubar);

	/*
	 * Create view stack with default elements
	 */
	Area             mouse_size(big_mouse.w, big_mouse.h);
	Mouse_cursor<PT> mouse_cursor((PT *)&big_mouse.pixels[0][0],
	                              mouse_size, &user_state);

	menubar.state(user_state, "", "", BLACK);

	Background background(screen.size());

	user_state.default_background(&background);
	user_state.stack(&mouse_cursor);
	user_state.stack(&menubar);
	user_state.stack(&background);

	/*
	 * Initialize Nitpicker root interface
	 */

	Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	static Nitpicker::Root<PT> np_root(&ep, Area(mode.width(), mode.height()),
	                                   &user_state, &sliced_heap,
	                                   &screen, &framebuffer,
	                                   MENUBAR_HEIGHT);

	env()->parent()->announce(ep.manage(&np_root));

	/*
	 * Initialize input handling
	 *
	 * We serialize the input handling with the client interfaces via
	 * Nitpicker's entry point. For this, we implement the input handling
	 * as a 'Rpc_object' and perform RPC calls to this local object in a
	 * periodic fashion.
	 */
	static Input_handler_component
		input_handler(&user_state, &mouse_cursor, mouse_size,
		              &screen, &input, &framebuffer, &timer);
	Capability<Input_handler> input_handler_cap = ep.manage(&input_handler);

	/* start periodic mode of operation */
	static Msgbuf<256> ih_snd_msg, ih_rcv_msg;
	Ipc_client input_handler_client(input_handler_cap, &ih_snd_msg, &ih_rcv_msg);
	while (1) {
		timer.msleep(10);
		input_handler_client << 0 << IPC_CALL;
	}

	sleep_forever();
	return 0;
}
