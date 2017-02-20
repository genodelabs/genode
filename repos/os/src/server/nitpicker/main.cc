/*
 * \brief  Nitpicker main program for Genode
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/log.h>
#include <base/component.h>
#include <base/allocator_guard.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <root/component.h>
#include <dataspace/client.h>
#include <timer_session/connection.h>
#include <input_session/connection.h>
#include <input_session/input_session.h>
#include <nitpicker_session/nitpicker_session.h>
#include <framebuffer_session/connection.h>
#include <util/color.h>
#include <os/pixel_rgb565.h>
#include <os/session_policy.h>
#include <os/reporter.h>

/* local includes */
#include "input.h"
#include "background.h"
#include "clip_guard.h"
#include "pointer_origin.h"
#include "domain_registry.h"

namespace Input       { class Session_component; }
namespace Framebuffer { class Session_component; }

namespace Nitpicker {
	class Session_component;
	template <typename> class Root;
	struct Main;
}

using Genode::size_t;
using Genode::Allocator;
using Genode::Entrypoint;
using Genode::List;
using Genode::Pixel_rgb565;
using Genode::strcmp;
using Genode::Env;
using Genode::Arg_string;
using Genode::Object_pool;
using Genode::Dataspace_capability;
using Genode::Session_label;
using Genode::Signal_transmitter;
using Genode::Signal_context_capability;
using Genode::Signal_handler;
using Genode::Attached_ram_dataspace;
using Genode::Attached_rom_dataspace;
using Genode::Attached_dataspace;
using Genode::Weak_ptr;
using Genode::Locked_ptr;


Framebuffer::Session *tmp_fb;


/***************
 ** Utilities **
 ***************/

static void report_session(Genode::Reporter &reporter, Session *session,
                           bool active = false)
{
	if (!reporter.enabled())
		return;

	Genode::Reporter::Xml_generator xml(reporter, [&] ()
	{
		if (session) {
			xml.attribute("label",  session->label().string());
			xml.attribute("domain", session->domain_name().string());

			Color const color = session->color();
			char buf[32];
			Genode::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
			                 color.r, color.g, color.b);
			xml.attribute("color", buf);

			if (active) xml.attribute("active", "yes");
		}
	});
}


/*
 * Font initialization
 */
extern char _binary_default_tff_start;

Text_painter::Font default_font(&_binary_default_tff_start);


template <typename PT>
struct Screen : public Canvas<PT>
{
	Screen(PT *base, Area size) : Canvas<PT>(base, size) { }
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
		Buffer(Genode::Ram_session &ram, Genode::Region_map &rm,
		       Area size, Framebuffer::Mode::Format format, Genode::size_t bytes)
		:
			_size(size), _format(format), _ram_ds(ram, rm, bytes)
		{ }

		/**
		 * Accessors
		 */
		Genode::Ram_dataspace_capability ds_cap() const { return _ram_ds.cap(); }
		Area                               size() const { return _size; }
		Framebuffer::Mode::Format        format() const { return _format; }
		void                        *local_addr() const { return _ram_ds.local_addr<void>(); }
};


/**
 * Interface for triggering the re-allocation of a virtual framebuffer
 *
 * Used by 'Framebuffer::Session_component',
 * implemented by 'Nitpicker::Session_component'
 */
struct Buffer_provider
{
	virtual Buffer *realloc_buffer(Framebuffer::Mode mode, bool use_alpha) = 0;
};


template <typename PT>
class Chunky_dataspace_texture : public Buffer,
                                 public Texture<PT>
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
		Chunky_dataspace_texture(Genode::Ram_session &ram, Genode::Region_map &rm,
		                         Area size, bool use_alpha)
		:
			Buffer(ram, rm, size, _format(), calc_num_bytes(size, use_alpha)),
			Texture<PT>((PT *)local_addr(),
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
			if (!Texture<PT>::alpha()) return 0;

			Area const size = Texture<PT>::size();

			/* input-mask values come right after the alpha values */
			return (unsigned char *)local_addr() + calc_num_bytes(size, false)
			                                     + size.count();
		}
};


/***********************
 ** Input sub session **
 ***********************/

class Input::Session_component : public Genode::Rpc_object<Session>
{
	public:

		enum { MAX_EVENTS = 200 };

		static size_t ev_ds_size() {
			return Genode::align_addr(MAX_EVENTS*sizeof(Event), 12); }

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
		unsigned   _num_ev = 0;

		Signal_context_capability _sigh;

	public:

		Session_component(Genode::Env &env)
		:
			_ev_ram_ds(env.ram(), env.rm(), ev_ds_size())
		{ }

		/**
		 * Wake up client
		 */
		void submit_signal()
		{
			if (_sigh.valid())
				Signal_transmitter(_sigh).submit();
		}

		/**
		 * Enqueue event into local event buffer of the input session
		 */
		void submit(const Event *ev)
		{
			/* drop event when event buffer is full */
			if (_num_ev >= MAX_EVENTS) return;

			/* insert event into local event buffer */
			_ev_buf[_num_ev++] = *ev;

			submit_signal();
		}


		/*****************************
		 ** Input session interface **
		 *****************************/

		Dataspace_capability dataspace() override { return _ev_ram_ds.cap(); }

		bool pending() const override { return _num_ev > 0; }

		int flush() override
		{
			unsigned ev_cnt;

			/* copy events from local event buffer to exported buffer */
			Event *ev_ds_buf = _ev_ram_ds.local_addr<Event>();
			for (ev_cnt = 0; ev_cnt < _num_ev; ev_cnt++)
				ev_ds_buf[ev_cnt] = _ev_buf[ev_cnt];

			_num_ev = 0;
			return ev_cnt;
		}

		void sigh(Genode::Signal_context_capability sigh) override { _sigh = sigh; }
};


/*****************************
 ** Framebuffer sub session **
 *****************************/

class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		::Buffer                 *_buffer = 0;
		View_stack               &_view_stack;
		::Session                &_session;
		Framebuffer::Session     &_framebuffer;
		Buffer_provider          &_buffer_provider;
		Signal_context_capability _mode_sigh;
		Signal_context_capability _sync_sigh;
		Framebuffer::Mode         _mode;
		bool                      _alpha = false;

	public:

		/**
		 * Constructor
		 */
		Session_component(View_stack           &view_stack,
		                  ::Session            &session,
		                  Framebuffer::Session &framebuffer,
		                  Buffer_provider      &buffer_provider)
		:
			_view_stack(view_stack),
			_session(session),
			_framebuffer(framebuffer),
			_buffer_provider(buffer_provider)
		{ }

		/**
		 * Change virtual framebuffer mode
		 *
		 * Called by Nitpicker::Session_component when re-dimensioning the
		 * buffer.
		 *
		 * The new mode does not immediately become active. The client can
		 * keep using an already obtained framebuffer dataspace. However,
		 * we inform the client about the mode change via a signal. If the
		 * client calls 'dataspace' the next time, the new mode becomes
		 * effective.
		 */
		void notify_mode_change(Framebuffer::Mode mode, bool alpha)
		{
			_mode  = mode;
			_alpha = alpha;

			if (_mode_sigh.valid())
				Signal_transmitter(_mode_sigh).submit();
		}

		void submit_sync()
		{
			if (_sync_sigh.valid())
				Signal_transmitter(_sync_sigh).submit();
		}


		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override
		{
			_buffer = _buffer_provider.realloc_buffer(_mode, _alpha);

			return _buffer ? _buffer->ds_cap() : Genode::Ram_dataspace_capability();
		}

		Mode mode() const override { return _mode; }

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
			Rect const rect(Point(x, y), Area(w, h));

			_view_stack.mark_session_views_as_dirty(_session, rect);
		}
};


/*****************************************
 ** Implementation of Nitpicker service **
 *****************************************/

class Nitpicker::Session_component : public Genode::Rpc_object<Session>,
                                     public ::Session,
                                     public Buffer_provider
{
	private:

		typedef ::View View;

		Env &_env;

		Genode::Allocator_guard _session_alloc;

		Framebuffer::Session &_framebuffer;

		/* Framebuffer_session_component */
		Framebuffer::Session_component _framebuffer_session_component;

		/* Input_session_component */
		Input::Session_component _input_session_component { _env };

		View_stack &_view_stack;

		Mode &_mode;

		Signal_context_capability _mode_sigh;

		View &_pointer_origin;

		List<Session_view_list_elem> _view_list;

		Genode::Tslab<View, 4000> _view_alloc { &_session_alloc };

		/* capabilities for sub sessions */
		Framebuffer::Session_capability _framebuffer_session_cap;
		Input::Session_capability       _input_session_cap;

		bool const _provides_default_bg;

		/* size of currently allocated virtual framebuffer, in bytes */
		size_t _buffer_size = 0;

		Attached_ram_dataspace _command_ds { _env.ram(), _env.rm(),
		                                     sizeof(Command_buffer) };

		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		typedef Genode::Handle_registry<View_handle, View> View_handle_registry;

		View_handle_registry _view_handle_registry;

		Genode::Reporter &_focus_reporter;

		void _release_buffer()
		{
			if (!::Session::texture())
				return;

			typedef Pixel_rgb565 PT;

			/* retrieve pointer to texture from session */
			Chunky_dataspace_texture<PT> const *cdt =
				static_cast<Chunky_dataspace_texture<PT> const *>(::Session::texture());

			::Session::texture(0, false);
			::Session::input_mask(0);

			destroy(&_session_alloc, const_cast<Chunky_dataspace_texture<PT> *>(cdt));

			_session_alloc.upgrade(_buffer_size);
			_buffer_size = 0;
		}

		/**
		 * Helper for performing sanity checks in OP_TO_FRONT and OP_TO_BACK
		 *
		 * We have to check for the equality of both the specified view and
		 * neighbor. If both arguments refer to the same view, the creation of
		 * locked pointers for both views would result in a deadlock.
		 */
		bool _views_are_equal(View_handle v1, View_handle v2)
		{
			if (!v1.valid() || !v2.valid())
				return false;

			Weak_ptr<View> v1_ptr = _view_handle_registry.lookup(v1);
			Weak_ptr<View> v2_ptr = _view_handle_registry.lookup(v2);

			return  v1_ptr == v2_ptr;
		}

		bool _focus_change_permitted() const
		{
			::Session * const focused_session = _mode.focused_session();

			/*
			 * If no session is focused, we allow any client to assign it. This
			 * is useful for programs such as an initial login window that
			 * should receive input events without prior manual selection via
			 * the mouse.
			 *
			 * In principle, a client could steal the focus during time between
			 * a currently focused session gets closed and before the user
			 * manually picks a new session. However, in practice, the focus
			 * policy during application startup and exit is managed by a
			 * window manager that sits between nitpicker and the application.
			 */
			if (!focused_session)
				return true;

			/*
			 * Check if the currently focused session label belongs to a
			 * session subordinated to the caller, i.e., it originated from
			 * a child of the caller or from the same process. This is the
			 * case if the first part of the focused session label is
			 * identical to the caller's label.
			 */
			char const * const focused_label = focused_session->label().string();
			char const * const caller_label  = label().string();

			return strcmp(focused_label, caller_label, Genode::strlen(caller_label)) == 0;
		}

		void _execute_command(Command const &command)
		{
			switch (command.opcode) {

			case Command::OP_GEOMETRY:
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(command.geometry.view));
					if (!view.valid())
						return;

					Point pos = command.geometry.rect.p1();

					/* transpose position of top-level views by vertical session offset */
					if (view->top_level())
						pos = ::Session::phys_pos(pos, _view_stack.size());

					if (view.valid())
						_view_stack.geometry(*view, Rect(pos, command.geometry.rect.area()));

					return;
				}

			case Command::OP_OFFSET:
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(command.geometry.view));

					if (view.valid())
						_view_stack.buffer_offset(*view, command.offset.offset);

					return;
				}

			case Command::OP_TO_FRONT:
				{
					if (_views_are_equal(command.to_front.view, command.to_front.neighbor))
						return;

					Locked_ptr<View> view(_view_handle_registry.lookup(command.to_front.view));
					if (!view.valid())
						return;

					/* bring to front if no neighbor is specified */
					if (!command.to_front.neighbor.valid()) {
						_view_stack.stack(*view, nullptr, true);
						return;
					}

					/* stack view relative to neighbor */
					Locked_ptr<View> neighbor(_view_handle_registry.lookup(command.to_front.neighbor));
					if (neighbor.valid())
						_view_stack.stack(*view, &(*neighbor), false);

					return;
				}

			case Command::OP_TO_BACK:
				{
					if (_views_are_equal(command.to_front.view, command.to_front.neighbor))
						return;

					Locked_ptr<View> view(_view_handle_registry.lookup(command.to_back.view));
					if (!view.valid())
						return;

					/* bring to front if no neighbor is specified */
					if (!command.to_front.neighbor.valid()) {
						_view_stack.stack(*view, nullptr, false);
						return;
					}

					/* stack view relative to neighbor */
					Locked_ptr<View> neighbor(_view_handle_registry.lookup(command.to_back.neighbor));
					if (neighbor.valid())
						_view_stack.stack(*view, &(*neighbor), true);

					return;
				}

			case Command::OP_BACKGROUND:
				{
					if (_provides_default_bg) {
						Locked_ptr<View> view(_view_handle_registry.lookup(command.to_front.view));
						if (!view.valid())
							return;

						view->background(true);
						_view_stack.default_background(*view);
						return;
					}

					/* revert old background view to normal mode */
					if (::Session::background())
						::Session::background()->background(false);

					/* assign session background */
					Locked_ptr<View> view(_view_handle_registry.lookup(command.to_front.view));
					if (!view.valid())
						return;

					::Session::background(&(*view));

					/* switch background view to background mode */
					if (::Session::background())
						view->background(true);

					return;
				}

			case Command::OP_TITLE:
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(command.title.view));

					if (view.valid())
						_view_stack.title(*view, command.title.title.string());

					return;
				}

			case Command::OP_NOP:
				return;
			}
		}

		void _destroy_view(View &view)
		{
			_view_stack.remove_view(view);
			_env.ep().dissolve(view);
			_view_list.remove(&view);
			destroy(_view_alloc, &view);
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Env                  &env,
		                  Session_label  const &label,
		                  View_stack           &view_stack,
		                  Mode                 &mode,
		                  View                 &pointer_origin,
		                  Framebuffer::Session &framebuffer,
		                  bool                  provides_default_bg,
		                  Allocator            &session_alloc,
		                  size_t                ram_quota,
		                  Genode::Reporter     &focus_reporter)
		:
			::Session(label),
			_env(env),
			_session_alloc(&session_alloc, ram_quota),
			_framebuffer(framebuffer),
			_framebuffer_session_component(view_stack, *this, framebuffer, *this),
			_view_stack(view_stack), _mode(mode),
			_pointer_origin(pointer_origin),
			_framebuffer_session_cap(_env.ep().manage(_framebuffer_session_component)),
			_input_session_cap(_env.ep().manage(_input_session_component)),
			_provides_default_bg(provides_default_bg),
			_view_handle_registry(_session_alloc),
			_focus_reporter(focus_reporter)
		{
			_session_alloc.upgrade(ram_quota);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			_env.ep().dissolve(_framebuffer_session_component);
			_env.ep().dissolve(_input_session_component);

			destroy_all_views();

			_release_buffer();
		}

		void destroy_all_views()
		{
			while (Session_view_list_elem *v = _view_list.first())
				_destroy_view(*static_cast<View *>(v));
		}

		/**
		 * Deliver mode-change signal to client
		 */
		void notify_mode_change()
		{
			if (_mode_sigh.valid())
				Signal_transmitter(_mode_sigh).submit();
		}

		void upgrade_ram_quota(size_t ram_quota) { _session_alloc.upgrade(ram_quota); }


		/**********************************
		 ** Nitpicker-internal interface **
		 **********************************/

		void submit_input_event(Input::Event e)
		{
			using namespace Input;

			Point const origin_offset =
				::Session::phys_pos(Point(0, 0), _view_stack.size());

			/*
			 * Transpose absolute coordinates by session-specific vertical
			 * offset.
			 */
			if (e.ax() || e.ay())
				e = Event(e.type(), e.code(),
				          Genode::max(0, e.ax() - origin_offset.x()),
				          Genode::max(0, e.ay() - origin_offset.y()),
				          e.rx(), e.ry());

			_input_session_component.submit(&e);
		}

		void submit_sync() override
		{
			_framebuffer_session_component.submit_sync();
		}


		/*********************************
		 ** Nitpicker session interface **
		 *********************************/

		Framebuffer::Session_capability framebuffer_session() override {
			return _framebuffer_session_cap; }

		Input::Session_capability input_session() override {
			return _input_session_cap; }

		View_handle create_view(View_handle parent_handle) override
		{
			View *view = nullptr;

			/*
			 * Create child view
			 */
			if (parent_handle.valid()) {

				try {
					Locked_ptr<View> parent(_view_handle_registry.lookup(parent_handle));
					if (!parent.valid())
						return View_handle();

					view = new (_view_alloc)
						View(*this,
						     View::NOT_TRANSPARENT, View::NOT_BACKGROUND,
						     &(*parent));

					parent->add_child(*view);
				}
				catch (View_handle_registry::Lookup_failed) {
					return View_handle(); }
				catch (View_handle_registry::Out_of_memory) {
					throw Nitpicker::Session::Out_of_metadata(); }
				catch (Genode::Allocator::Out_of_memory) {
					throw Nitpicker::Session::Out_of_metadata(); }
			}

			/*
			 * Create top-level view
			 */
			else {
				try {
					view = new (_view_alloc)
						View(*this,
						     View::NOT_TRANSPARENT, View::NOT_BACKGROUND,
						     nullptr);
					}
				catch (Genode::Allocator::Out_of_memory) {
					throw Nitpicker::Session::Out_of_metadata(); }
			}

			view->apply_origin_policy(_pointer_origin);

			_view_list.insert(view);
			_env.ep().manage(*view);

			try { return _view_handle_registry.alloc(*view); }
			catch (View_handle_registry::Out_of_memory) {
				throw Nitpicker::Session::Out_of_metadata(); }
		}

		void destroy_view(View_handle handle) override
		{
			/*
			 * Search view object given the handle
			 *
			 * We cannot look up the view directly from the
			 * '_view_handle_registry' because we would obtain a weak
			 * pointer to the view object. If we called the object's
			 * destructor from the corresponding locked pointer, the
			 * call of 'lock_for_destruction' in the view's destructor
			 * would attempt to take the lock again.
			 */
			for (Session_view_list_elem *v = _view_list.first(); v; v = v->next()) {

				try {
					View &view = *static_cast<View *>(v);
					if (_view_handle_registry.has_handle(view, handle)) {
						_destroy_view(view);
						break;
					}
				} catch (View_handle_registry::Lookup_failed) { }
			}

			_view_handle_registry.free(handle);
		}

		View_handle view_handle(View_capability view_cap, View_handle handle) override
		{
			auto lambda = [&] (View *view)
			{
				return (view) ? _view_handle_registry.alloc(*view, handle)
				              : View_handle();
			};

			try { return _env.ep().rpc_ep().apply(view_cap, lambda); }
			catch (View_handle_registry::Out_of_memory) {
				throw Nitpicker::Session::Out_of_metadata(); }
		}

		View_capability view_capability(View_handle handle) override
		{
			try {
				Locked_ptr<View> view(_view_handle_registry.lookup(handle));
				return view.valid() ? view->cap() : View_capability();
			}
			catch (View_handle_registry::Lookup_failed) {
				return View_capability();
			}
		}

		void release_view_handle(View_handle handle) override
		{
			try {
				_view_handle_registry.free(handle); }

			catch (View_handle_registry::Lookup_failed) {
				Genode::warning("view lookup failed while releasing view handle");
				return;
			}
		}

		Genode::Dataspace_capability command_dataspace() override
		{
			return _command_ds.cap();
		}

		void execute() override
		{
			for (unsigned i = 0; i < _command_buffer.num(); i++) {
				try {
					_execute_command(_command_buffer.get(i)); }
				catch (View_handle_registry::Lookup_failed) {
					Genode::warning("view lookup failed during command execution"); }
			}
		}

		Framebuffer::Mode mode() override
		{
			Area const phys_area(_framebuffer.mode().width(),
			                     _framebuffer.mode().height());

			Area const session_area = ::Session::screen_area(phys_area);

			return Framebuffer::Mode(session_area.w(), session_area.h(),
			                         _framebuffer.mode().format());
		}

		void mode_sigh(Signal_context_capability sigh) override
		{
			_mode_sigh = sigh;
		}

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			/* check if the session quota suffices for the specified mode */
			if (_session_alloc.quota() < ram_quota(mode, use_alpha))
				throw Nitpicker::Session::Out_of_metadata();

			_framebuffer_session_component.notify_mode_change(mode, use_alpha);
		}

		void focus(Genode::Capability<Nitpicker::Session> session_cap) override
		{
			/* check permission by comparing session labels */
			if (!_focus_change_permitted()) {
				Genode::warning("unauthorized focus change requesed by ", label().string());
				return;
			}

			/* lookup targeted session object */
			auto lambda = [this] (Session_component *session)
			{
				_mode.next_focused_session(session);
			};
			_env.ep().rpc_ep().apply(session_cap, lambda);

			/*
			 * To avoid changing the focus in the middle of a drag operation,
			 * we cannot perform the focus change immediately. Instead, it
			 * comes into effect via the 'Mode::apply_pending_focus_change()'
			 * function called the next time when the user input is handled and
			 * no drag operation is in flight.
			 */
		}

		void session_control(Label suffix, Session_control control) override
		{
			Session_label const selector(label(), suffix);

			switch (control) {
			case SESSION_CONTROL_HIDE:     _view_stack.visible(selector.string(), false); break;
			case SESSION_CONTROL_SHOW:     _view_stack.visible(selector.string(), true);  break;
			case SESSION_CONTROL_TO_FRONT: _view_stack.to_front(selector.string());       break;
			}
		}


		/*******************************
		 ** Buffer_provider interface **
		 *******************************/

		Buffer *realloc_buffer(Framebuffer::Mode mode, bool use_alpha)
		{
			typedef Pixel_rgb565 PT;

			Area const size(mode.width(), mode.height());

			_buffer_size =
				Chunky_dataspace_texture<PT>::calc_num_bytes(size, use_alpha);

			/*
			 * Preserve the content of the original buffer if nitpicker has
			 * enough lack memory to temporarily keep the original pixels.
			 */
			Texture<PT> const *src_texture = nullptr;
			if (::Session::texture()) {

				enum { PRESERVED_RAM = 128*1024 };
				if (_env.ram().avail() > _buffer_size + PRESERVED_RAM) {
					src_texture = static_cast<Texture<PT> const *>(::Session::texture());
				} else {
					Genode::warning("not enough RAM to preserve buffer content during resize");
					_release_buffer();
				}
			}

			Chunky_dataspace_texture<PT> * const texture = new (&_session_alloc)
				Chunky_dataspace_texture<PT>(_env.ram(), _env.rm(), size, use_alpha);

			/* copy old buffer content into new buffer and release old buffer */
			if (src_texture) {

				Genode::Surface<PT> surface(texture->pixel(),
				                            texture->Texture_base::size());

				Texture_painter::paint(surface, *src_texture, Color(), Point(0, 0),
				                       Texture_painter::SOLID, false);
				_release_buffer();
			}

			if (!_session_alloc.withdraw(_buffer_size)) {
				destroy(&_session_alloc, texture);
				_buffer_size = 0;
				return 0;
			}

			::Session::texture(texture, use_alpha);
			::Session::input_mask(texture->input_mask_buffer());

			return texture;
		}
};


template <typename PT>
class Nitpicker::Root : public Genode::Root_component<Session_component>
{
	private:

		Env                          &_env;
		Attached_rom_dataspace const &_config;
		Session_list                 &_session_list;
		Domain_registry const        &_domain_registry;
		Global_keys                  &_global_keys;
		Framebuffer::Mode             _scr_mode;
		View_stack                   &_view_stack;
		Mode                         &_mode;
		::View                       &_pointer_origin;
		Framebuffer::Session         &_framebuffer;
		Genode::Reporter             &_focus_reporter;

	protected:

		Session_component *_create_session(const char *args)
		{
			size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			size_t const required_quota = Input::Session_component::ev_ds_size()
			                            + Genode::align_addr(sizeof(Session::Command_buffer), 12);

			if (ram_quota < required_quota) {
				Genode::warning("Insufficient dontated ram_quota (", ram_quota,
				                " bytes), require ", required_quota, " bytes");
				throw Root::Quota_exceeded();
			}

			size_t const unused_quota = ram_quota - required_quota;

			Genode::Session_label const label = Genode::label_from_args(args);
			bool const provides_default_bg = (label == "backdrop");

			Session_component *session = new (md_alloc())
				Session_component(_env, label, _view_stack, _mode,
				                  _pointer_origin, _framebuffer,
				                  provides_default_bg, *md_alloc(), unused_quota,
				                  _focus_reporter);

			session->apply_session_policy(_config.xml(), _domain_registry);
			_session_list.insert(session);
			_global_keys.apply_config(_config.xml(), _session_list);

			return session;
		}

		void _upgrade_session(Session_component *s, const char *args)
		{
			size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			s->upgrade_ram_quota(ram_quota);
		}

		void _destroy_session(Session_component *session)
		{
			_session_list.remove(session);
			_global_keys.apply_config(_config.xml(), _session_list);

			session->destroy_all_views();
			_mode.forget(*session);

			Genode::destroy(md_alloc(), session);
		}

	public:

		/**
		 * Constructor
		 */
		Root(Env &env, Attached_rom_dataspace const &config,
		     Session_list &session_list, Domain_registry const &domain_registry,
		     Global_keys &global_keys, View_stack &view_stack, Mode &mode,
		     ::View &pointer_origin, Allocator &md_alloc,
		     Framebuffer::Session &framebuffer, Genode::Reporter &focus_reporter)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _config(config),
			_session_list(session_list), _domain_registry(domain_registry),
			_global_keys(global_keys), _view_stack(view_stack), _mode(mode),
			_pointer_origin(pointer_origin), _framebuffer(framebuffer),
			_focus_reporter(focus_reporter)
		{ }
};


struct Nitpicker::Main
{
	Env &env;

	/*
	 * Sessions to the required external services
	 */
	Framebuffer::Connection framebuffer { env, Framebuffer::Mode() };
	Input::Connection       input { env };

	Input::Event * const ev_buf = env.rm().attach(input.dataspace());

	typedef Pixel_rgb565 PT;  /* physical pixel type */

	/*
	 * Initialize framebuffer
	 *
	 * The framebuffer is encapsulated in a volatile object to allow its
	 * reconstruction at runtime as a response to resolution changes.
	 */
	struct Framebuffer_screen
	{
		Framebuffer::Session &framebuffer;

		Framebuffer::Mode const mode = framebuffer.mode();

		Attached_dataspace fb_ds;

		Screen<PT> screen = { fb_ds.local_addr<PT>(), Area(mode.width(), mode.height()) };

		/**
		 * Constructor
		 */
		Framebuffer_screen(Genode::Region_map &rm, Framebuffer::Session &fb)
		: framebuffer(fb), fb_ds(rm, framebuffer.dataspace()) { }
	};

	Genode::Reconstructible<Framebuffer_screen> fb_screen = { env.rm(), framebuffer };

	void handle_fb_mode();

	Signal_handler<Main> fb_mode_handler = { env.ep(), *this, &Main::handle_fb_mode };

	/*
	 * User-input policy
	 */
	Global_keys global_keys;

	Session_list session_list;

	/*
	 * Construct empty domain registry. The initial version will be replaced
	 * on the first call of 'handle_config'.
	 */
	Genode::Heap domain_registry_heap { env.ram(), env.rm() };
	Genode::Reconstructible<Domain_registry> domain_registry {
		domain_registry_heap, Genode::Xml_node("<config/>") };

	User_state user_state = { global_keys, fb_screen->screen.size() };

	/*
	 * Create view stack with default elements
	 */
	Pointer_origin pointer_origin;

	Background background = { Area(99999, 99999) };

	/*
	 * Initialize Nitpicker root interface
	 */
	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Genode::Reporter pointer_reporter = { env, "pointer" };
	Genode::Reporter hover_reporter   = { env, "hover" };
	Genode::Reporter focus_reporter   = { env, "focus" };

	Genode::Attached_rom_dataspace config { env, "config" };

	Root<PT> np_root = { env, config, session_list, *domain_registry,
	                     global_keys, user_state, user_state, pointer_origin,
	                     sliced_heap, framebuffer, focus_reporter };

	/*
	 * Configuration-update handler, executed in the context of the RPC
	 * entrypoint.
	 *
	 * In addition to installing the signal handler, we trigger first signal
	 * manually to turn the initial configuration into effect.
	 */
	void handle_config();

	Signal_handler<Main> config_handler = { env.ep(), *this, &Main::handle_config};

	/**
	 * Signal handler invoked on the reception of user input
	 */
	void handle_input();

	Signal_handler<Main> input_handler = { env.ep(), *this, &Main::handle_input };

	/*
	 * Dispatch input and redraw periodically
	 */
	Timer::Connection timer { env };

	/**
	 * Counter that is incremented periodically
	 */
	unsigned period_cnt = 0;

	/**
	 * Period counter when the user was active the last time
	 */
	unsigned last_active_period = 0;

	/**
	 * Number of periods after the last user activity when we regard the user
	 * as becoming inactive
	 */
	unsigned activity_threshold = 50;

	/**
	 * True if the user was recently active
	 *
	 * This state is reported as part of focus reports to allow the clipboard
	 * to dynamically adjust its information-flow policy to the user activity.
	 */
	bool user_active = false;

	/**
	 * Perform redraw and flush pixels to the framebuffer
	 */
	void draw_and_flush()
	{
		user_state.draw(fb_screen->screen).flush([&] (Rect const &rect) {
			framebuffer.refresh(rect.x1(), rect.y1(),
			                    rect.w(),  rect.h()); });
	}

	Main(Env &env) : env(env)
	{
		user_state.default_background(background);
		user_state.stack(pointer_origin);
		user_state.stack(background);

		config.sigh(config_handler);
		handle_config();

		framebuffer.sync_sigh(input_handler);
		framebuffer.mode_sigh(fb_mode_handler);

		env.parent().announce(env.ep().manage(np_root));
	}
};


void Nitpicker::Main::handle_input()
{
	period_cnt++;

	Point       const old_pointer_pos     = user_state.pointer_pos();
	::Session * const old_pointed_session = user_state.pointed_session();
	::Session * const old_focused_session = user_state.Mode::focused_session();
	bool        const old_user_active     = user_active;

	/* handle batch of pending events */
	if (import_input_events(ev_buf, input.flush(), user_state)) {
		last_active_period = period_cnt;
		user_active        = true;
	}

	user_state.Mode::apply_pending_focus_change();

	Point       const new_pointer_pos     = user_state.pointer_pos();
	::Session * const new_pointed_session = user_state.pointed_session();
	::Session * const new_focused_session = user_state.Mode::focused_session();

	if (old_focused_session != new_focused_session)
		user_state.update_all_views();

	/* flag user as inactive after activity threshold is reached */
	if (period_cnt == last_active_period + activity_threshold)
		user_active = false;

	/* report mouse-position updates */
	if (pointer_reporter.enabled() && old_pointer_pos != new_pointer_pos) {

		Genode::Reporter::Xml_generator xml(pointer_reporter, [&] ()
		{
			xml.attribute("xpos", new_pointer_pos.x());
			xml.attribute("ypos", new_pointer_pos.y());
		});
	}

	/* report hover changes */
	if (!user_state.Mode::key_pressed()
	 && old_pointed_session != new_pointed_session) {
		report_session(hover_reporter, new_pointed_session);
	}

	/* report focus changes */
	if (old_focused_session != new_focused_session
	 || old_user_active     != user_active)
		report_session(focus_reporter, new_focused_session, user_active);

	/* update mouse cursor */
	if (old_pointer_pos != new_pointer_pos)
		user_state.geometry(pointer_origin, Rect(new_pointer_pos, Area()));

	/* perform redraw and flush pixels to the framebuffer */
	user_state.draw(fb_screen->screen).flush([&] (Rect const &rect) {
		framebuffer.refresh(rect.x1(), rect.y1(),
		                    rect.w(),  rect.h()); });

	user_state.mark_all_views_as_clean();

	/* deliver framebuffer synchronization events */
	for (::Session *s = session_list.first(); s; s = s->next())
		s->submit_sync();
}


/**
 * Helper function for 'handle_config'
 */
static void configure_reporter(Genode::Xml_node config, Genode::Reporter &reporter)
{
	try {
		reporter.enabled(config.sub_node("report")
		                       .attribute_value(reporter.name().string(), false));
	} catch (...) {
		reporter.enabled(false);
	}
}


void Nitpicker::Main::handle_config()
{
	config.update();

	/* update global keys policy */
	global_keys.apply_config(config.xml(), session_list);

	/* update background color */
	try {
		config.xml().sub_node("background")
		.attribute("color").value(&background.color);
	} catch (...) { }

	/* enable or disable redraw debug mode */
	tmp_fb = config.xml().attribute_value("flash", false)
		? &framebuffer
		: nullptr;

	configure_reporter(config.xml(), pointer_reporter);
	configure_reporter(config.xml(), hover_reporter);
	configure_reporter(config.xml(), focus_reporter);

	/* update domain registry and session policies */
	for (::Session *s = session_list.first(); s; s = s->next())
		s->reset_domain();

	try {
		domain_registry.construct(domain_registry_heap, config.xml()); }
	catch (...) { }

	for (::Session *s = session_list.first(); s; s = s->next())
		s->apply_session_policy(config.xml(), *domain_registry);

	user_state.apply_origin_policy(pointer_origin);

	/*
	 * Domains may have changed their layering, resort the view stack with the
	 * new constrains.
	 */
	user_state.sort_views_by_layer();

	/* redraw */
	user_state.update_all_views();
}


void Nitpicker::Main::handle_fb_mode()
{
	/* reconstruct framebuffer screen and menu bar */
	fb_screen.construct(env.rm(), framebuffer);

	/* let the view stack use the new size */
	user_state.size(Area(fb_screen->mode.width(), fb_screen->mode.height()));

	/* redraw */
	user_state.update_all_views();

	/* notify clients about the change screen mode */
	for (::Session *s = session_list.first(); s; s = s->next()) {
		Session_component *sc = dynamic_cast<Session_component *>(s);
		if (sc)
			sc->notify_mode_change();
	}
}


void Component::construct(Genode::Env &env) { static Nitpicker::Main nitpicker(env); }
