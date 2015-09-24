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
#include <base/allocator_guard.h>
#include <os/attached_ram_dataspace.h>
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
#include <os/server.h>
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
using Genode::Rpc_entrypoint;
using Genode::List;
using Genode::Pixel_rgb565;
using Genode::strcmp;
using Genode::config;
using Genode::env;
using Genode::Arg_string;
using Genode::Object_pool;
using Genode::Dataspace_capability;
using Genode::Session_label;
using Genode::Signal_transmitter;
using Genode::Signal_context_capability;
using Genode::Signal_rpc_member;
using Genode::Attached_ram_dataspace;
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
	if (!reporter.is_enabled())
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


static void report_kill_focus(Genode::Reporter &reporter)
{
	if (!reporter.is_enabled())
		return;

	Genode::Reporter::Xml_generator xml(reporter, [&] ()
	{
		xml.attribute("label",  "");
		xml.attribute("domain", "");
		xml.attribute("color",  "#ff4444");
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
		Buffer(Area size, Framebuffer::Mode::Format format, Genode::size_t bytes)
		:
			_size(size), _format(format),
			_ram_ds(env()->ram_session(), bytes)
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
		Chunky_dataspace_texture(Area size, bool use_alpha)
		:
			Buffer(size, _format(), calc_num_bytes(size, use_alpha)),
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
		Attached_ram_dataspace _ev_ram_ds = { env()->ram_session(), ev_ds_size() };

		/*
		 * Local event buffer that is copied
		 * to the exported event buffer when
		 * flush() gets called.
		 */
		Event      _ev_buf[MAX_EVENTS];
		unsigned   _num_ev = 0;

		Signal_context_capability _sigh;

	public:

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

		bool is_pending() const override { return _num_ev > 0; }

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

		Genode::Allocator_guard _session_alloc;

		Framebuffer::Session &_framebuffer;

		/* Framebuffer_session_component */
		Framebuffer::Session_component _framebuffer_session_component;

		/* Input_session_component */
		Input::Session_component _input_session_component;

		/*
		 * Entrypoint that is used for the views, input session,
		 * and framebuffer session.
		 */
		Rpc_entrypoint &_ep;

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

		Attached_ram_dataspace _command_ds { env()->ram_session(),
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
					if (!view.is_valid())
						return;

					Point pos = command.geometry.rect.p1();

					/* transpose position of top-level views by vertical session offset */
					if (view->top_level())
						pos = ::Session::phys_pos(pos, _view_stack.size());

					if (view.is_valid())
						_view_stack.geometry(*view, Rect(pos, command.geometry.rect.area()));

					return;
				}

			case Command::OP_OFFSET:
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(command.geometry.view));

					if (view.is_valid())
						_view_stack.buffer_offset(*view, command.offset.offset);

					return;
				}

			case Command::OP_TO_FRONT:
				{
					if (_views_are_equal(command.to_front.view, command.to_front.neighbor))
						return;

					Locked_ptr<View> view(_view_handle_registry.lookup(command.to_front.view));
					if (!view.is_valid())
						return;

					/* bring to front if no neighbor is specified */
					if (!command.to_front.neighbor.valid()) {
						_view_stack.stack(*view, nullptr, true);
						return;
					}

					/* stack view relative to neighbor */
					Locked_ptr<View> neighbor(_view_handle_registry.lookup(command.to_front.neighbor));
					if (neighbor.is_valid())
						_view_stack.stack(*view, &(*neighbor), false);

					return;
				}

			case Command::OP_TO_BACK:
				{
					if (_views_are_equal(command.to_front.view, command.to_front.neighbor))
						return;

					Locked_ptr<View> view(_view_handle_registry.lookup(command.to_back.view));
					if (!view.is_valid())
						return;

					/* bring to front if no neighbor is specified */
					if (!command.to_front.neighbor.valid()) {
						_view_stack.stack(*view, nullptr, false);
						return;
					}

					/* stack view relative to neighbor */
					Locked_ptr<View> neighbor(_view_handle_registry.lookup(command.to_back.neighbor));
					if (neighbor.is_valid())
						_view_stack.stack(*view, &(*neighbor), true);

					return;
				}

			case Command::OP_BACKGROUND:
				{
					if (_provides_default_bg) {
						Locked_ptr<View> view(_view_handle_registry.lookup(command.to_front.view));
						if (!view.is_valid())
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
					if (!view.is_valid())
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

					if (view.is_valid())
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
			_ep.dissolve(&view);
			_view_list.remove(&view);
			destroy(_view_alloc, &view);
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Session_label  const &label,
		                  View_stack           &view_stack,
		                  Mode                 &mode,
		                  View                 &pointer_origin,
		                  Rpc_entrypoint       &ep,
		                  Framebuffer::Session &framebuffer,
		                  bool                  provides_default_bg,
		                  Allocator            &session_alloc,
		                  size_t                ram_quota,
		                  Genode::Reporter     &focus_reporter)
		:
			::Session(label),
			_session_alloc(&session_alloc, ram_quota),
			_framebuffer(framebuffer),
			_framebuffer_session_component(view_stack, *this, framebuffer, *this),
			_ep(ep), _view_stack(view_stack), _mode(mode),
			_pointer_origin(pointer_origin),
			_framebuffer_session_cap(_ep.manage(&_framebuffer_session_component)),
			_input_session_cap(_ep.manage(&_input_session_component)),
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
			_ep.dissolve(&_framebuffer_session_component);
			_ep.dissolve(&_input_session_component);

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
					if (!parent.is_valid())
						return View_handle();

					view = new (_view_alloc)
						View(*this,
						     View::NOT_TRANSPARENT, View::NOT_BACKGROUND,
						     &(*parent));

					parent->add_child(*view);
				}
				catch (View_handle_registry::Lookup_failed) {
					return View_handle(); }
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
			_ep.manage(view);

			return _view_handle_registry.alloc(*view);
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
			return _ep.apply(view_cap, lambda);
		}

		View_capability view_capability(View_handle handle) override
		{
			try {
				Locked_ptr<View> view(_view_handle_registry.lookup(handle));
				return view.is_valid() ? view->cap() : View_capability();
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
				PWRN("view lookup failed while releasing view handle");
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
					PWRN("view lookup failed during command execution"); }
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
				PWRN("unauthorized focus change requesed by %s", label().string());
				return;
			}

			/* prevent focus changes during drag operations */
			if (_mode.drag())
				return;

			/* lookup targeted session object */
			auto lambda = [this] (Session_component *session)
			{
				_mode.focused_session(session);
				report_session(_focus_reporter, session);
			};
			_ep.apply(session_cap, lambda);
		}

		void session_control(Label suffix, Session_control control) override
		{
			char selector[Label::size()];

			Genode::snprintf(selector, sizeof(selector), "%s%s%s",
			                 label().string(),
			                 suffix.length() ? " -> " : "", suffix.string());

			switch (control) {
			case SESSION_CONTROL_HIDE:     _view_stack.visible(selector, false); break;
			case SESSION_CONTROL_SHOW:     _view_stack.visible(selector, true);  break;
			case SESSION_CONTROL_TO_FRONT: _view_stack.to_front(selector);       break;
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
				if (env()->ram_session()->avail() > _buffer_size + PRESERVED_RAM) {
					src_texture = static_cast<Texture<PT> const *>(::Session::texture());
				} else {
					PWRN("not enough RAM to preserve buffer content during resize");
					_release_buffer();
				}
			}

			Chunky_dataspace_texture<PT> * const texture =
				new (&_session_alloc) Chunky_dataspace_texture<PT>(size, use_alpha);

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

		Session_list          &_session_list;
		Domain_registry const &_domain_registry;
		Global_keys           &_global_keys;
		Framebuffer::Mode      _scr_mode;
		View_stack            &_view_stack;
		Mode                  &_mode;
		::View                &_pointer_origin;
		Framebuffer::Session  &_framebuffer;
		Genode::Reporter      &_focus_reporter;

	protected:

		Session_component *_create_session(const char *args)
		{
			PINF("create session with args: %s\n", args);
			size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			size_t const required_quota = Input::Session_component::ev_ds_size()
			                            + Genode::align_addr(sizeof(Session::Command_buffer), 12);

			if (ram_quota < required_quota) {
				PWRN("Insufficient dontated ram_quota (%zd bytes), require %zd bytes",
				     ram_quota, required_quota);
				throw Root::Quota_exceeded();
			}

			size_t const unused_quota = ram_quota - required_quota;

			Session_label const label(args);
			bool const provides_default_bg = (strcmp(label.string(), "backdrop") == 0);

			Session_component *session = new (md_alloc())
				Session_component(Session_label(args), _view_stack, _mode,
				                  _pointer_origin, *ep(), _framebuffer,
				                  provides_default_bg, *md_alloc(), unused_quota,
				                  _focus_reporter);

			session->apply_session_policy(_domain_registry);
			_session_list.insert(session);
			_global_keys.apply_config(_session_list);

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
			_global_keys.apply_config(_session_list);

			session->destroy_all_views();
			_mode.forget(*session);

			destroy(md_alloc(), session);
		}

	public:

		/**
		 * Constructor
		 */
		Root(Session_list &session_list,
		     Domain_registry const &domain_registry, Global_keys &global_keys,
		     Rpc_entrypoint &session_ep, View_stack &view_stack, Mode &mode,
		     ::View &pointer_origin, Allocator &md_alloc,
		     Framebuffer::Session &framebuffer, Genode::Reporter &focus_reporter)
		:
			Root_component<Session_component>(&session_ep, &md_alloc),
			_session_list(session_list), _domain_registry(domain_registry),
			_global_keys(global_keys), _view_stack(view_stack), _mode(mode),
			_pointer_origin(pointer_origin), _framebuffer(framebuffer),
			_focus_reporter(focus_reporter)
		{ }
};


struct Nitpicker::Main
{
	Server::Entrypoint &ep;

	/*
	 * Sessions to the required external services
	 */
	Framebuffer::Connection framebuffer;
	Input::Connection       input;

	Input::Event * const ev_buf =
		env()->rm_session()->attach(input.dataspace());

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

		Attached_dataspace fb_ds = { framebuffer.dataspace() };

		Screen<PT> screen = { fb_ds.local_addr<PT>(), Area(mode.width(), mode.height()) };

		/**
		 * Constructor
		 */
		Framebuffer_screen(Framebuffer::Session &fb) : framebuffer(fb) { }
	};

	Genode::Volatile_object<Framebuffer_screen> fb_screen = { framebuffer };

	void handle_fb_mode(unsigned);

	Signal_rpc_member<Main> fb_mode_dispatcher = { ep, *this, &Main::handle_fb_mode };

	/*
	 * User-input policy
	 */
	Global_keys global_keys;

	Session_list session_list;

	/*
	 * Construct empty domain registry. The initial version will be replaced
	 * on the first call of 'handle_config'.
	 */
	Genode::Volatile_object<Domain_registry> domain_registry {
		*env()->heap(), Genode::Xml_node("<config/>") };

	User_state user_state = { global_keys, fb_screen->screen.size() };

	/*
	 * Create view stack with default elements
	 */
	Pointer_origin pointer_origin;

	Background background = { Area(99999, 99999) };

	/*
	 * Initialize Nitpicker root interface
	 */
	Genode::Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Genode::Reporter pointer_reporter = { "pointer" };
	Genode::Reporter hover_reporter   = { "hover" };
	Genode::Reporter focus_reporter   = { "focus" };
	Genode::Reporter xray_reporter    = { "xray" };

	Root<PT> np_root = { session_list, *domain_registry, global_keys,
	                     ep.rpc_ep(), user_state, user_state, pointer_origin,
	                     sliced_heap, framebuffer, focus_reporter };

	/*
	 * Configuration-update dispatcher, executed in the context of the RPC
	 * entrypoint.
	 *
	 * In addition to installing the signal dispatcher, we trigger first signal
	 * manually to turn the initial configuration into effect.
	 */
	void handle_config(unsigned);

	Signal_rpc_member<Main> config_dispatcher = { ep, *this, &Main::handle_config};

	/**
	 * Signal handler invoked on the reception of user input
	 */
	void handle_input(unsigned);

	Signal_rpc_member<Main> input_dispatcher = { ep, *this, &Main::handle_input };

	/*
	 * Dispatch input and redraw periodically
	 */
	Timer::Connection timer;

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

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		user_state.default_background(background);
		user_state.stack(pointer_origin);
		user_state.stack(background);

		config()->sigh(config_dispatcher);
		handle_config(0);

		timer.sigh(input_dispatcher);
		timer.trigger_periodic(10*1000);

		framebuffer.mode_sigh(fb_mode_dispatcher);

		env()->parent()->announce(ep.manage(np_root));
	}
};


void Nitpicker::Main::handle_input(unsigned)
{
	period_cnt++;

	/*
	 * If kill mode is already active, we got recursively called from
	 * within this 'input_func' (via 'wait_and_dispatch_one_signal').
	 * In this case, return immediately. New input events will be
	 * processed in the local 'do' loop.
	 */
	if (user_state.kill())
		return;

	do {
		Point       const old_pointer_pos     = user_state.pointer_pos();
		::Session * const old_pointed_session = user_state.pointed_session();
		::Session * const old_focused_session = user_state.Mode::focused_session();
		bool        const old_kill_mode       = user_state.kill();
		bool        const old_xray_mode       = user_state.xray();
		bool        const old_user_active     = user_active;

		/* handle batch of pending events */
		if (import_input_events(ev_buf, input.flush(), user_state)) {
			last_active_period = period_cnt;
			user_active        = true;
		}

		Point       const new_pointer_pos     = user_state.pointer_pos();
		::Session * const new_pointed_session = user_state.pointed_session();
		::Session * const new_focused_session = user_state.Mode::focused_session();
		bool        const new_kill_mode       = user_state.kill();
		bool        const new_xray_mode       = user_state.xray();

		/* flag user as inactive after activity threshold is reached */
		if (period_cnt == last_active_period + activity_threshold)
			user_active = false;

		/* report mouse-position updates */
		if (pointer_reporter.is_enabled() && old_pointer_pos != new_pointer_pos) {

			Genode::Reporter::Xml_generator xml(pointer_reporter, [&] ()
			{
				xml.attribute("xpos", new_pointer_pos.x());
				xml.attribute("ypos", new_pointer_pos.y());
			});
		}

		if (xray_reporter.is_enabled() && old_xray_mode != new_xray_mode) {

			Genode::Reporter::Xml_generator xml(xray_reporter, [&] ()
			{
				xml.attribute("enabled", new_xray_mode ? "yes" : "no");
			});
		}

		/* report hover changes */
		if (old_pointed_session != new_pointed_session)
			report_session(hover_reporter, new_pointed_session);

		/* report focus changes */
		if (old_focused_session != new_focused_session
		 || old_user_active     != user_active)
			report_session(focus_reporter, new_focused_session, user_active);

		/* report kill mode */
		if (old_kill_mode != new_kill_mode) {

			if (new_kill_mode)
				report_kill_focus(focus_reporter);

			if (!new_kill_mode)
				report_session(focus_reporter, new_focused_session);
		}

		/*
		 * Continuously redraw the whole screen when kill mode is active.
		 * Otherwise client updates (e.g., the status bar) would stay invisible
		 * because we do not dispatch the RPC interface during kill mode.
		 */
		if (new_kill_mode)
			user_state.update_all_views();

		/* update mouse cursor */
		if (old_pointer_pos != new_pointer_pos)
			user_state.geometry(pointer_origin, Rect(new_pointer_pos, Area()));

		/* perform redraw and flush pixels to the framebuffer */
		user_state.draw(fb_screen->screen).flush([&] (Rect const &rect) {
			framebuffer.refresh(rect.x1(), rect.y1(),
			                    rect.w(),  rect.h()); });

		user_state.mark_all_views_as_clean();

		/* deliver framebuffer synchronization events */
		if (!user_state.kill()) {
			for (::Session *s = session_list.first(); s; s = s->next())
				s->submit_sync();
		}

		/*
		 * In kill mode, we do not leave the dispatch function in order to
		 * block RPC calls from Nitpicker clients. We block for signals
		 * here to stay responsive to user input and configuration changes.
		 * Nested calls of 'input_func' are prevented by the condition
		 * check for 'user_state.kill()' at the beginning of the handler.
		 */
		if (user_state.kill())
			Server::wait_and_dispatch_one_signal();

	} while (user_state.kill());
}


/**
 * Helper function for 'handle_config'
 */
static void configure_reporter(Genode::Reporter &reporter)
{
	try {
		Genode::Xml_node config_xml = Genode::config()->xml_node();
		reporter.enabled(config_xml.sub_node("report")
		                           .attribute(reporter.name().string())
		                           .has_value("yes"));
	} catch (...) {
		reporter.enabled(false);
	}
}


void Nitpicker::Main::handle_config(unsigned)
{
	config()->reload();

	/* update global keys policy */
	global_keys.apply_config(session_list);

	/* update background color */
	try {
		config()->xml_node().sub_node("background")
		.attribute("color").value(&background.color);
	} catch (...) { }

	/* enable or disable redraw debug mode */
	try {
		tmp_fb = nullptr;
		if (config()->xml_node().attribute("flash").has_value("yes"))
			tmp_fb = &framebuffer;
	} catch (...) { }

	configure_reporter(pointer_reporter);
	configure_reporter(hover_reporter);
	configure_reporter(focus_reporter);
	configure_reporter(xray_reporter);

	/* update domain registry and session policies */
	for (::Session *s = session_list.first(); s; s = s->next())
		s->reset_domain();

	try {
		domain_registry.construct(*env()->heap(), config()->xml_node()); }
	catch (...) { }

	for (::Session *s = session_list.first(); s; s = s->next())
		s->apply_session_policy(*domain_registry);

	user_state.apply_origin_policy(pointer_origin);

	/*
	 * Domains may have changed their layering, resort the view stack with the
	 * new constrains.
	 */
	user_state.sort_views_by_layer();

	/* redraw */
	user_state.update_all_views();
}


void Nitpicker::Main::handle_fb_mode(unsigned)
{
	/* reconstruct framebuffer screen and menu bar */
	fb_screen.construct(framebuffer);

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


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "nitpicker_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Nitpicker::Main nitpicker(ep);
	}
}
