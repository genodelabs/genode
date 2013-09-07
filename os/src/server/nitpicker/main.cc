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
#include <os/signal_rpc_dispatcher.h>

/* local includes */
#include "input.h"
#include "big_mouse.h"
#include "background.h"
#include "clip_guard.h"
#include "mouse_cursor.h"
#include "chunky_menubar.h"

namespace Input       { using namespace Genode;  }
namespace Input       { class Session_component; }
namespace Framebuffer { using namespace Genode;  }
namespace Framebuffer { class Session_component; }
namespace Nitpicker   { using namespace Genode;  }
namespace Nitpicker   { class Session_component; }
namespace Nitpicker   { template <typename> class Root; }


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
	using namespace Genode;

	/* use white by default */
	Color color = WHITE;

	try {
		Session_policy policy(session_args);

		/* read color attribute */
		policy.attribute("color").value(&color);
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

class Input::Session_component : public Genode::Rpc_object<Session>
{
	public:

		enum { MAX_EVENTS = 200 };

	private:

		/*
		 * Exported event buffer dataspace
		 */
		Attached_ram_dataspace _ev_ram_ds;

		/*
		 * Local event buffer that is copied
		 * to the exported event buffer when
		 * flush() gets called.
		 */
		Event      _ev_buf[MAX_EVENTS];
		unsigned   _num_ev;

	public:

		static size_t ev_ds_size() {
			return align_addr(MAX_EVENTS*sizeof(Event), 12); }

		/**
		 * Constructor
		 */
		Session_component():
			_ev_ram_ds(env()->ram_session(), ev_ds_size()),
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

		Dataspace_capability dataspace() { return _ev_ram_ds.cap(); }

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


/*****************************
 ** Framebuffer sub session **
 *****************************/

class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		::Buffer             &_buffer;
		View_stack           &_view_stack;
		::Session            &_session;
		Flush_merger         &_flush_merger;
		Framebuffer::Session &_framebuffer;

	public:

		/**
		 * Constructor
		 *
		 * \param session  Nitpicker session
		 */
		Session_component(::Buffer &buffer, View_stack &view_stack,
		                  ::Session &session, Flush_merger &flush_merger,
		                  Framebuffer::Session &framebuffer)
		:
			_buffer(buffer), _view_stack(view_stack), _session(session),
			_flush_merger(flush_merger), _framebuffer(framebuffer) { }

		Dataspace_capability dataspace() { return _buffer.ds_cap(); }

		void release() { }

		Mode mode() const
		{
			return Mode(_buffer.size().w(), _buffer.size().h(),
			            _buffer.format());
		}

		void mode_sigh(Signal_context_capability) { }

		void refresh(int x, int y, int w, int h)
		{
			_view_stack.update_session_views(_session,
			                                 Rect(Point(x, y), Area(w, h)));

			/* flush dirty pixels to physical frame buffer */
			if (_flush_merger.defer == false) {
				Rect r = _flush_merger.to_be_flushed();
				_framebuffer.refresh(r.x1(), r.y1(), r.w(), r.h());
				_flush_merger.reset();
			}
			_flush_merger.defer = true;
		}
};


class View_component : public Genode::List<View_component>::Element,
                       public Genode::Rpc_object<Nitpicker::View>
{
	private:

		typedef Genode::Rpc_entrypoint Rpc_entrypoint;

		View_stack     &_view_stack;
		::View          _view;
		Rpc_entrypoint &_ep;

	public:

		/**
		 * Constructor
		 */
		View_component(::Session &session, View_stack &view_stack,
		               Rpc_entrypoint &ep):
			_view_stack(view_stack),
			_view(session,
			      session.stay_top() ? ::View::STAY_TOP : ::View::NOT_STAY_TOP,
			      ::View::NOT_TRANSPARENT, ::View::NOT_BACKGROUND, Rect()),
			_ep(ep) { }

		::View &view() { return _view; }


		/******************************
		 ** Nitpicker view interface **
		 ******************************/

		int viewport(int x, int y, int w, int h,
		             int buf_x, int buf_y, bool redraw)
		{
			/* transpose y position by vertical session offset */
			y += _view.session().v_offset();

			_view_stack.viewport(_view, Rect(Point(x, y), Area(w, h)),
			                            Point(buf_x, buf_y), redraw);
			return 0;
		}

		int stack(Nitpicker::View_capability neighbor_cap, bool behind, bool redraw)
		{
			using namespace Genode;

			Object_pool<View_component>::Guard nvc(_ep.lookup_and_lock(neighbor_cap));

			::View *neighbor_view = nvc ? &nvc->view() : 0;

			_view_stack.stack(_view, neighbor_view, behind, redraw);
			return 0;
		}

		int title(Title const &title)
		{
			_view_stack.title(_view, title.string());
			return 0;
		}
};


/*****************************************
 ** Implementation of Nitpicker service **
 *****************************************/

class Nitpicker::Session_component : public Genode::Rpc_object<Session>,
                                     public ::Session
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
		Rpc_entrypoint &_ep;

		View_stack &_view_stack;

		List<View_component> _view_list;

		/* capabilities for sub sessions */
		Framebuffer::Session_capability _framebuffer_session_cap;
		Input::Session_capability       _input_session_cap;

		bool _provides_default_bg;

	public:

		/**
		 * Constructor
		 */
		Session_component(char             const *name,
		                  ::Buffer               &buffer,
		                  Texture          const &texture,
		                  View_stack             &view_stack,
		                  Rpc_entrypoint         &ep,
		                  Flush_merger           &flush_merger,
		                  Framebuffer::Session   &framebuffer,
		                  int                     v_offset,
		                  unsigned char    const *input_mask,
		                  bool                    provides_default_bg,
		                  Color                   color,
		                  bool                    stay_top)
		:
			::Session(name, texture, v_offset, color, input_mask, stay_top),
			_framebuffer_session_component(buffer, view_stack, *this, flush_merger, framebuffer),
			_ep(ep), _view_stack(view_stack),
			_framebuffer_session_cap(_ep.manage(&_framebuffer_session_component)),
			_input_session_cap(_ep.manage(&_input_session_component)),
			_provides_default_bg(provides_default_bg)
		{ }

		/**
		 * Destructor
		 */
		~Session_component()
		{
			_ep.dissolve(&_framebuffer_session_component);
			_ep.dissolve(&_input_session_component);

			while (View_component *vc = _view_list.first())
				destroy_view(vc->cap());
		}


		/******************************************
		 ** Nitpicker-internal session interface **
		 ******************************************/

		void submit_input_event(Input::Event e)
		{
			using namespace Input;

			/*
			 * Transpose absolute coordinates by session-specific vertical
			 * offset.
			 */
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
			View_component *view = new (env()->heap())
			                       View_component(*this, _view_stack, _ep);

			_view_list.insert(view);
			return _ep.manage(view);
		}

		void destroy_view(View_capability view_cap)
		{
			View_component *vc = dynamic_cast<View_component *>(_ep.lookup_and_lock(view_cap));
			if (!vc) return;

			_view_stack.remove_view(vc->view());
			_ep.dissolve(vc);
			_view_list.remove(vc);
			destroy(env()->heap(), vc);
		}

		int background(View_capability view_cap)
		{
			if (_provides_default_bg) {
				Object_pool<View_component>::Guard vc(_ep.lookup_and_lock(view_cap));
				vc->view().background(true);
				_view_stack.default_background(vc->view());
				return 0;
			}

			/* revert old background view to normal mode */
			if (::Session::background()) ::Session::background()->background(false);

			/* assign session background */
			Object_pool<View_component>::Guard vc(_ep.lookup_and_lock(view_cap));
			::Session::background(&vc->view());

			/* switch background view to background mode */
			if (::Session::background()) vc->view().background(true);

			return 0;
		}
};


template <typename PT>
class Nitpicker::Root : public Genode::Root_component<Session_component>
{
	private:

		Session_list         &_session_list;
		Global_keys          &_global_keys;
		Area                  _scr_size;
		View_stack           &_view_stack;
		Flush_merger         &_flush_merger;
		Framebuffer::Session &_framebuffer;
		int                   _default_v_offset;

	protected:

		Session_component *_create_session(const char *args)
		{
			PINF("create session with args: %s\n", args);
			size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			int v_offset = _default_v_offset;

			/* determine buffer size of the session */
			Area size(Arg_string::find_arg(args, "fb_width" ).long_value(_scr_size.w()),
			          Arg_string::find_arg(args, "fb_height").long_value(_scr_size.h() - v_offset));

			char label_buf[::Session::LABEL_LEN];
			Arg_string::find_arg(args, "label").string(label_buf, sizeof(label_buf), "<unlabeled>");

			bool use_alpha = Arg_string::find_arg(args, "alpha").bool_value(false);
			bool stay_top  = Arg_string::find_arg(args, "stay_top").bool_value(false);

			size_t texture_num_bytes = Chunky_dataspace_texture<PT>::calc_num_bytes(size, use_alpha);

			size_t required_quota = texture_num_bytes
			                      + Input::Session_component::ev_ds_size();

			if (ram_quota < required_quota) {
				PWRN("Insufficient dontated ram_quota (%zd bytes), require %zd bytes",
				     ram_quota, required_quota);
				throw Root::Quota_exceeded();
			}

			/* allocate texture */
			Chunky_dataspace_texture<PT> *cdt;
			cdt = new (md_alloc()) Chunky_dataspace_texture<PT>(size, use_alpha);

			bool provides_default_bg = (strcmp(label_buf, "backdrop") == 0);

			Session_component *session = new (md_alloc())
				Session_component(label_buf, *cdt, *cdt, _view_stack, *ep(),
				                  _flush_merger, _framebuffer, v_offset,
				                  cdt->input_mask_buffer(),
				                  provides_default_bg, session_color(args),
				                  stay_top);

			_session_list.insert(session);
			_global_keys.apply_config(_session_list);

			return session;
		}

		void _destroy_session(Session_component *session)
		{
			/* retrieve pointer to texture from session */
			Chunky_dataspace_texture<PT> const &cdt =
				static_cast<Chunky_dataspace_texture<PT> const &>(session->texture());

			_session_list.remove(session);
			_global_keys.apply_config(_session_list);

			destroy(md_alloc(), session);

			/* cast away constness just for destruction of the texture */
			destroy(md_alloc(), const_cast<Chunky_dataspace_texture<PT> *>(&cdt));
		}

	public:

		/**
		 * Constructor
		 */
		Root(Session_list &session_list, Global_keys &global_keys,
		     Rpc_entrypoint &session_ep, Area scr_size,
		     View_stack &view_stack, Allocator &md_alloc,
		     Flush_merger &flush_merger,
		     Framebuffer::Session &framebuffer, int default_v_offset)
		:
			Root_component<Session_component>(&session_ep, &md_alloc),
			_session_list(session_list), _global_keys(global_keys),
			_scr_size(scr_size), _view_stack(view_stack), _flush_merger(flush_merger),
			_framebuffer(framebuffer), _default_v_offset(default_v_offset) { }
};


void wait_and_dispatch_one_signal(Genode::Signal_receiver &sig_rec)
{
	using namespace Genode;

	/*
	 * We call the signal dispatcher outside of the scope of 'Signal'
	 * object because we block the RPC interface in the input handler
	 * when the kill mode gets actived. While kill mode is active, we
	 * do not serve incoming RPC requests but we need to stay responsive
	 * to user input. Hence, we wait for signals in the input dispatcher
	 * in this case. An already existing 'Signal' object would lock the
	 * signal receiver and thereby prevent this nested way of signal
	 * handling.
	 */
	Signal_dispatcher_base *dispatcher = 0;
	unsigned num = 0;

	{
		Signal sig = sig_rec.wait_for_signal();
		dispatcher = dynamic_cast<Signal_dispatcher_base *>(sig.context());
		num        = sig.num();
	}

	if (dispatcher)
		dispatcher->dispatch(num);
}


/******************
 ** Main program **
 ******************/

typedef Pixel_rgb565 PT;  /* physical pixel type */


int main(int argc, char **argv)
{
	using namespace Genode;

	/*
	 * Sessions to the required external services
	 */
	static Framebuffer::Connection framebuffer;
	static Input::Connection       input;
	static Cap_connection          cap;

	static Input::Event * const ev_buf =
		env()->rm_session()->attach(input.dataspace());

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

	static Global_keys global_keys;

	static Session_list session_list;

	static User_state user_state(global_keys, screen, menubar);

	/*
	 * Create view stack with default elements
	 */
	Area             mouse_size(big_mouse.w, big_mouse.h);
	Mouse_cursor<PT> mouse_cursor((PT *)&big_mouse.pixels[0][0],
	                              mouse_size, user_state);

	menubar.state(user_state, "", "", BLACK);

	Background background(screen.size());

	user_state.default_background(background);
	user_state.stack(mouse_cursor);
	user_state.stack(menubar);
	user_state.stack(background);

	/*
	 * Initialize Nitpicker root interface
	 */
	Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	static Nitpicker::Root<PT> np_root(session_list, global_keys,
	                                   ep, Area(mode.width(), mode.height()),
	                                   user_state, sliced_heap,
	                                   screen, framebuffer,
	                                   MENUBAR_HEIGHT);

	static Signal_receiver sig_rec;

	/*
	 * Configuration-update dispatcher, executed in the context of the RPC
	 * entrypoint.
	 *
	 * In addition to installing the signal dispatcher, we trigger first signal
	 * manually to turn the initial configuration into effect.
	 */
	static auto config_func = [&](unsigned)
	{
		config()->reload();
		global_keys.apply_config(session_list);
		try {
			config()->xml_node().sub_node("background")
			                    .attribute("color").value(&background.color);
		} catch (...) { }
		user_state.update_all_views();
	};

	auto config_dispatcher = signal_rpc_dispatcher(config_func);

	Signal_context_capability config_sigh = config_dispatcher.manage(sig_rec, ep);

	config()->sigh(config_sigh);

	Signal_transmitter(config_sigh).submit();

	/*
	 * Input dispatcher, executed in the contect of the RPC entrypoint
	 */
	static auto input_func = [&](unsigned num)
	{
		/*
		 * If kill mode is already active, we got recursively called from
		 * within this 'input_func' (via 'wait_and_dispatch_one_signal').
		 * In this case, return immediately. New input events will be
		 * processed in the local 'do' loop.
		 */
		if (user_state.kill())
			return;

		do {
			Point const old_mouse_pos = user_state.mouse_pos();

			/* handle batch of pending events */
			if (input.is_pending())
				import_input_events(ev_buf, input.flush(), user_state);

			Point const new_mouse_pos = user_state.mouse_pos();

			/* update mouse cursor */
			if (old_mouse_pos != new_mouse_pos)
				user_state.viewport(mouse_cursor,
				                    Rect(new_mouse_pos, mouse_size),
				                    Point(), true);

			/* flush dirty pixels to physical frame buffer */
			if (screen.defer == false) {
				Rect const r = screen.to_be_flushed();
				if (r.valid())
					framebuffer.refresh(r.x1(), r.y1(), r.w(), r.h());
				screen.reset();
			}
			screen.defer = false;

			/*
			 * In kill mode, we do not leave the dispatch function in order to
			 * block RPC calls from Nitpicker clients. We block for signals
			 * here to stay responsive to user input and configuration changes.
			 * Nested calls of 'input_func' are prevented by the condition
			 * check for 'user_state.kill()' at the beginning of the handler.
			 */
			if (user_state.kill())
				wait_and_dispatch_one_signal(sig_rec);

		} while (user_state.kill());
	};

	auto input_dispatcher = signal_rpc_dispatcher(input_func);

	/*
	 * Dispatch input on periodic timer signals every 10 milliseconds
	 */
	static Timer::Connection timer;
	timer.sigh(input_dispatcher.manage(sig_rec, ep));
	timer.trigger_periodic(10*1000);

	env()->parent()->announce(ep.manage(&np_root));

	for (;;)
		wait_and_dispatch_one_signal(sig_rec);

	return 0;
}
