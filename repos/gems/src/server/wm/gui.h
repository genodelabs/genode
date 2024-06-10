/*
 * \brief  Virtualized GUI service announced to the outside world
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GUI_H_
#define _GUI_H_

/* Genode includes */
#include <util/list.h>
#include <base/tslab.h>
#include <os/surface.h>
#include <base/attached_ram_dataspace.h>
#include <os/session_policy.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <gui_session/connection.h>
#include <input_session/capability.h>
#include <input/component.h>

/* local includes */
#include <window_registry.h>
#include <decorator_gui.h>
#include <layouter_gui.h>
#include <direct_gui.h>


namespace Wm {

	using Genode::Rpc_object;
	using Genode::List;
	using Genode::Allocator;
	using Genode::Affinity;
	using Genode::static_cap_cast;
	using Genode::Signal_handler;
	using Genode::Weak_ptr;
	using Genode::Locked_ptr;
	using Genode::Tslab;
	using Genode::Attached_ram_dataspace;
	using Genode::Signal_context_capability;
	using Genode::Signal_transmitter;
	using Genode::Reporter;
	using Genode::Capability;
	using Genode::Interface;
}

namespace Wm { namespace Gui {

	using namespace ::Gui;

	class Click_handler;
	class Input_origin_changed_handler;
	class View_handle_ctx;
	class View;
	class Top_level_view;
	class Child_view;
	class Session_control_fn;
	class Session_component;
	class Root;

	using Rect          = Genode::Surface_base::Rect;
	using Point         = Genode::Surface_base::Point;
	using Session_label = Genode::Session_label;
} }


/**
 * Interface used for propagating clicks into unfocused windows to the layouter
 *
 * The click handler is invoked only for those click events that are of
 * interest to the layouter. In particular, a click into an unfocused window
 * may trigger the layouter to raise the window and change the focus. However,
 * clicks into an already focused window should be of no interest to the
 * layouter. So we hide them from the layouter.
 */
struct Wm::Gui::Click_handler : Interface
{
	virtual void handle_click(Point pos) = 0;
};


/**
 * Called by a top-level view to propagate the need to update the virtual
 * pointer position of a client when the client's window moved.
 */
struct Wm::Gui::Input_origin_changed_handler : Interface
{
	virtual void input_origin_changed() = 0;
};


struct Gui::View : Genode::Interface { GENODE_RPC_INTERFACE(); };


class Wm::Gui::View : private Genode::Weak_object<View>,
                      public  Genode::Rpc_object< ::Gui::View>
{
	private:

		friend class Genode::Weak_ptr<View>;
		friend class Genode::Locked_ptr<View>;

	protected:

		using Title       = Genode::String<100>;
		using Command     = Gui::Session::Command;
		using View_handle = Gui::Session::View_handle;

		Session_label        _session_label;
		Gui::Session_client &_real_gui;
		View_handle          _real_handle     { };
		Title                _title           { };
		Rect                 _geometry        { };
		Point                _buffer_offset   { };
		Weak_ptr<View>       _neighbor_ptr    { };
		bool                 _neighbor_behind { };
		bool                 _has_alpha;

		View(Gui::Session_client &real_gui,
		     Session_label const &session_label,
		     bool                 has_alpha)
		:
			_session_label(session_label), _real_gui(real_gui),
			_has_alpha(has_alpha)
		{ }

		/**
		 * Propagate cached view geometry to the physical GUI view
		 */
		virtual void _propagate_view_geometry() = 0;

		/**
		 * Apply cached view state to the physical GUI view
		 */
		void _unsynchronized_apply_view_config(Locked_ptr<View> &neighbor)
		{
			if (!_real_handle.valid())
				return;

			_propagate_view_geometry();
			_real_gui.enqueue<Command::Offset>(_real_handle, _buffer_offset);
			_real_gui.enqueue<Command::Title> (_real_handle, _title.string());

			View_handle real_neighbor_handle;

			if (neighbor.valid())
				real_neighbor_handle = _real_gui.view_handle(neighbor->real_view_cap());

			if (_neighbor_behind)
				_real_gui.enqueue<Command::To_front>(_real_handle, real_neighbor_handle);
			else
				_real_gui.enqueue<Command::To_back>(_real_handle, real_neighbor_handle);

			_real_gui.execute();

			if (real_neighbor_handle.valid())
				_real_gui.release_view_handle(real_neighbor_handle);
		}

		void _apply_view_config()
		{
			Locked_ptr<View> neighbor(_neighbor_ptr);
			_unsynchronized_apply_view_config(neighbor);
		}

	public:

		~View()
		{
			if (_real_handle.valid())
				_real_gui.destroy_view(_real_handle);
		}

		using Genode::Weak_object<View>::weak_ptr;
		using Genode::Weak_object<View>::lock_for_destruction;

		Point virtual_position() const { return _geometry.at; }

		virtual bool belongs_to_win_id(Window_registry::Id id) const = 0;

		virtual void geometry(Rect geometry)
		{
			_geometry = geometry;

			/*
			 * Propagate new size to real GUI view but
			 */
			if (_real_handle.valid()) {
				_propagate_view_geometry();
				_real_gui.execute();
			}
		}

		virtual void title(char const *title)
		{
			_title = Title(title);

			if (_real_handle.valid()) {
				_real_gui.enqueue<Command::Title>(_real_handle, title);
				_real_gui.execute();
			}
		}

		virtual Point input_anchor_position() const = 0;

		virtual void stack(Weak_ptr<View>, bool) { }

		View_handle real_handle() const { return _real_handle; }

		View_capability real_view_cap()
		{
			return _real_gui.view_capability(_real_handle);
		}

		void buffer_offset(Point buffer_offset)
		{
			_buffer_offset = buffer_offset;

			if (_real_handle.valid()) {
				_real_gui.enqueue<Command::Offset>(_real_handle, _buffer_offset);
				_real_gui.execute();
			}
		}

		bool has_alpha() const { return _has_alpha; }
};


class Wm::Gui::Top_level_view : public View, private List<Top_level_view>::Element
{
	private:

		friend class List<Top_level_view>;

		Window_registry::Id _win_id { };

		Window_registry &_window_registry;

		Input_origin_changed_handler &_input_origin_changed_handler;

		/*
		 * Geometry of window-content view, which corresponds to the location
		 * of the window content as known by the decorator.
		 */
		Rect _content_geometry { };

		bool _resizeable = false;

		Title         _window_title { };
		Session_label _session_label;

		using Command = Gui::Session::Command;

	public:

		Top_level_view(Gui::Session_client          &real_gui,
		               Session_label          const &session_label,
		               bool                          has_alpha,
		               Window_registry              &window_registry,
		               Input_origin_changed_handler &input_origin_changed_handler)
		:
			View(real_gui, session_label, has_alpha),
			_window_registry(window_registry),
			_input_origin_changed_handler(input_origin_changed_handler),
			_session_label(session_label)
		{ }

		~Top_level_view()
		{
			if (_win_id.valid())
				_window_registry.destroy(_win_id);

			View::lock_for_destruction();
		}

		using List<Top_level_view>::Element::next;

		void _propagate_view_geometry() override { }

		void geometry(Rect geometry) override
		{
			/*
			 * Add window to the window-list model on the first call. We
			 * defer the creation of the window ID until the time when the
			 * initial geometry is known.
			 */
			if (!_win_id.valid()) {
				_win_id = _window_registry.create();
				_window_registry.title(_win_id, _window_title.string());
				_window_registry.label(_win_id, _session_label);
				_window_registry.has_alpha(_win_id, View::has_alpha());
				_window_registry.resizeable(_win_id, _resizeable);
			}

			_window_registry.size(_win_id, geometry.area);

			View::geometry(geometry);
		}

		Area size() const { return _geometry.area; }

		void title(char const *title) override
		{
			View::title(title);

			_window_title = Title(title);

			if (_win_id.valid())
				_window_registry.title(_win_id, _window_title.string());
		}

		bool has_win_id(Window_registry::Id id) const { return id == _win_id; }

		bool belongs_to_win_id(Window_registry::Id id) const override
		{
			return has_win_id(id);
		}

		Point input_anchor_position() const override
		{
			return _content_geometry.at;
		}

		void content_geometry(Rect rect)
		{
			bool const position_changed = _content_geometry.at != rect.at;

			_content_geometry = rect;

			if (position_changed)
				_input_origin_changed_handler.input_origin_changed();
		}

		View_capability content_view()
		{
			if (!_real_handle.valid()) {

				/*
				 * Create and configure physical GUI view.
				 */
				_real_handle = _real_gui.create_view();

				_real_gui.enqueue<Command::Offset>(_real_handle, _buffer_offset);
				_real_gui.enqueue<Command::Title> (_real_handle, _title.string());
				_real_gui.execute();
			}

			return _real_gui.view_capability(_real_handle);
		}

		void hidden(bool hidden) { _window_registry.hidden(_win_id, hidden); }

		void resizeable(bool resizeable)
		{
			_resizeable = resizeable;

			if (_win_id.valid())
				_window_registry.resizeable(_win_id, resizeable);
		}
};


class Wm::Gui::Child_view : public View, private List<Child_view>::Element
{
	private:

		friend class List<Child_view>;

		Weak_ptr<View> mutable _parent;

	public:

		Child_view(Gui::Session_client &real_gui,
		           Session_label const &session_label,
		           bool                 has_alpha,
		           Weak_ptr<View>       parent)
		:
			View(real_gui, session_label, has_alpha), _parent(parent)
		{
			try_to_init_real_view();
		}

		~Child_view()
		{
			View::lock_for_destruction();
		}

		using List<Child_view>::Element::next;

		void _propagate_view_geometry() override
		{
			_real_gui.enqueue<Command::Geometry>(_real_handle, _geometry);
		}

		void stack(Weak_ptr<View> neighbor_ptr, bool behind) override
		{
			_neighbor_ptr    = neighbor_ptr;
			_neighbor_behind = behind;

			_apply_view_config();
		}

		bool belongs_to_win_id(Window_registry::Id id) const override
		{
			Locked_ptr<View> parent(_parent);
			return parent.valid() && parent->belongs_to_win_id(id);
		}

		Point input_anchor_position() const override
		{
			Locked_ptr<View> parent(_parent);
			if (parent.valid())
				return parent->input_anchor_position();

			return Point();
		}

		void try_to_init_real_view()
		{
			if (_real_handle.valid())
				return;

			Locked_ptr<View> parent(_parent);
			if (!parent.valid())
				return;

			View_handle parent_handle = _real_gui.view_handle(parent->real_view_cap());
			if (!parent_handle.valid())
				return;

			_real_handle = _real_gui.create_child_view(parent_handle);

			_real_gui.release_view_handle(parent_handle);

			if (_neighbor_ptr == _parent)
				_unsynchronized_apply_view_config(parent);
			else
				_apply_view_config();
		}

		void update_child_stacking()
		{
			_apply_view_config();
		}

		void hide()
		{
			if (!_real_handle.valid())
				return;

			_real_gui.destroy_view(_real_handle);
			_real_handle = { };
		}
};


class Wm::Gui::Session_component : public Rpc_object<Gui::Session>,
                                   private List<Session_component>::Element,
                                   private Input_origin_changed_handler
{
	private:

		friend class List<Session_component>;

		using View_handle = Gui::Session::View_handle;

		Genode::Env &_env;

		Session_label          _session_label;
		Genode::Ram_allocator &_ram;
		Gui::Connection        _session { _env, _session_label.string() };

		Window_registry             &_window_registry;
		Tslab<Top_level_view, 8000>  _top_level_view_alloc;
		Tslab<Child_view, 6000>      _child_view_alloc;
		List<Top_level_view>         _top_level_views { };
		List<Child_view>             _child_views { };
		Input::Session_component     _input_session { _env, _ram };
		Input::Session_capability    _input_session_cap;
		Click_handler               &_click_handler;
		Signal_context_capability    _mode_sigh { };
		Area                         _requested_size { };
		bool                         _resize_requested = false;
		bool                         _has_alpha = false;
		Pointer::State               _pointer_state;
		Point                  const _initial_pointer_pos { -1, -1 };
		Point                        _pointer_pos = _initial_pointer_pos;
		Point                        _virtual_pointer_pos { };
		unsigned                     _key_cnt = 0;

		/*
		 * Command buffer
		 */
		using Command_buffer = Gui::Session::Command_buffer;

		Attached_ram_dataspace _command_ds { _ram, _env.rm(), sizeof(Command_buffer) };

		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		/*
		 * View handle registry
		 */
		using View_handle_registry = Genode::Handle_registry<View_handle, View>;

		View_handle_registry _view_handle_registry;

		/*
		 * Input
		 */
		Input::Session_client _gui_input    { _env.rm(), _session.input_session() };
		Attached_dataspace    _gui_input_ds { _env.rm(), _gui_input.dataspace() };

		Signal_handler<Session_component> _input_handler {
			_env.ep(), *this, &Session_component::_handle_input };

		Signal_handler<Session_component> _mode_handler {
			_env.ep(), *this, &Session_component::_handle_mode_change };

		Point _input_origin() const
		{
			if (Top_level_view const *v = _top_level_views.first())
				return v->virtual_position() - v->input_anchor_position();

			if (Child_view const *v = _child_views.first())
				return Point(0, 0) - v->input_anchor_position();

			return Point();
		}

		/**
		 * Translate input event to the client's coordinate system
		 */
		Input::Event _translate_event(Input::Event ev, Point const origin)
		{
			ev.handle_absolute_motion([&] (int x, int y) {
				Point const p = Point(x, y) + origin;
				ev = Input::Absolute_motion{p.x, p.y};
			});

			ev.handle_touch([&] (Input::Touch_id id, float x, float y) {
				ev = Input::Touch { .id = id,
				                    .x  = x + (float)origin.x,
				                    .y  = y + (float)origin.y }; });

			return ev;
		}

		bool _click_into_unfocused_view(Input::Event const ev)
		{
			/*
			 * XXX check if unfocused
			 *
			 * Right now, we report more butten events to the layouter
			 * than the layouter really needs.
			 */
			if (ev.key_press(Input::BTN_LEFT))
				return true;

			return false;
		}

		bool _first_motion = true;

		void _handle_input()
		{
			Point const input_origin = _input_origin();

			Input::Event const * const events =
				_gui_input_ds.local_addr<Input::Event>();

			while (_gui_input.pending()) {

				size_t const num_events = _gui_input.flush();

				/*
				 * We trust the nitpicker GUI server to return a valid number
				 * of events.
				 */

				for (size_t i = 0; i < num_events; i++) {

					Input::Event const ev = events[i];

					if (ev.press())   _key_cnt++;
					if (ev.release()) _key_cnt--;

					/* keep track of pointer position when hovering */
					ev.handle_absolute_motion([&] (int x, int y) {
						_pointer_pos = Point(x, y); });

					/* propagate layout-affecting events to the layouter */
					if (_click_into_unfocused_view(ev) && _key_cnt == 1)
						_click_handler.handle_click(_pointer_pos);

					/*
					 * Hide application-local motion events from the pointer
					 * position shared with the decorator. The position is
					 * propagated only when entering/leaving an application's
					 * screen area or when finishing a drag operation.
					 */
					bool propagate_to_pointer_state = false;

					/* pointer enters application area */
					if (ev.absolute_motion() && _first_motion && _key_cnt == 0) {
						propagate_to_pointer_state = true;
						_first_motion = false;
					}

					/* may be end of drag operation */
					if (ev.press() || ev.release())
						propagate_to_pointer_state = true;

					/* pointer has left the application area */
					if (ev.hover_leave()) {
						_pointer_pos  = _initial_pointer_pos;
						_first_motion = true;
						propagate_to_pointer_state = true;
					}

					if (propagate_to_pointer_state)
						_pointer_state.apply_event(ev);

					/* submit event to the client */
					_input_session.submit(_translate_event(ev, input_origin));
				}
			}
		}

		void _handle_mode_change()
		{
			/*
			 * Inform a viewless client about the upstream
			 * mode change.
			 */
			if (_mode_sigh.valid() && !_top_level_views.first())
				Signal_transmitter(_mode_sigh).submit();
		}

		/**
		 * Input_origin_changed_handler interface
		 */
		void input_origin_changed() override
		{
			if (_pointer_pos == _initial_pointer_pos)
				return;

			Point const pos = _pointer_pos + _input_origin();

			_input_session.submit(Input::Absolute_motion { pos.x, pos.y });
		}

		View &_create_view_object()
		{
			Top_level_view *view = new (_top_level_view_alloc)
				Top_level_view(_session, _session_label, _has_alpha,
				               _window_registry, *this);

			view->resizeable(_mode_sigh.valid());

			_top_level_views.insert(view);
			return *view;
		}

		View &_create_child_view_object(View_handle parent_handle)
		{
			Weak_ptr<View> parent_ptr = _view_handle_registry.lookup(parent_handle);

			Child_view *view = new (_child_view_alloc)
				Child_view(_session, _session_label, _has_alpha, parent_ptr);

			_child_views.insert(view);
			return *view;
		}

		void _destroy_top_level_view(Top_level_view &view)
		{
			_top_level_views.remove(&view);
			_env.ep().dissolve(view);
			Genode::destroy(&_top_level_view_alloc, &view);
		}

		void _destroy_child_view(Child_view &view)
		{
			_child_views.remove(&view);
			_env.ep().dissolve(view);
			Genode::destroy(&_child_view_alloc, &view);
		}

		void _destroy_view_object(View &view)
		{
			if (Top_level_view *v = dynamic_cast<Top_level_view *>(&view))
				_destroy_top_level_view(*v);

			if (Child_view *v = dynamic_cast<Child_view *>(&view))
				_destroy_child_view(*v);
		}

		void _execute_command(Command const &command)
		{
			switch (command.opcode) {

			case Command::OP_GEOMETRY:
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(command.geometry.view));
					if (view.valid())
						view->geometry(command.geometry.rect);
					return;
				}

			case Command::OP_OFFSET:
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(command.offset.view));
					if (view.valid())
						view->buffer_offset(command.offset.offset);

					return;
				}

			case Command::OP_TO_FRONT:
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(command.to_front.view));
					if (!view.valid())
						return;

					/* bring to front if no neighbor is specified */
					if (!command.to_front.neighbor.valid()) {
						view->stack(Weak_ptr<View>(), true);
						return;
					}

					/* stack view relative to neighbor */
					view->stack(_view_handle_registry.lookup(command.to_front.neighbor),
					            true);
					return;
				}

			case Command::OP_TO_BACK:
				{
					return;
				}

			case Command::OP_BACKGROUND:
				{
					return;
				}

			case Command::OP_TITLE:
				{
					char sanitized_title[command.title.title.capacity()];

					Genode::copy_cstring(sanitized_title, command.title.title.string(),
					                     sizeof(sanitized_title));

					for (char *c = sanitized_title; *c; c++)
						if (*c == '"')
							*c = '\'';

					Locked_ptr<View> view(_view_handle_registry.lookup(command.title.view));
					if (view.valid())
						view->title(sanitized_title);

					return;
				}

			case Command::OP_NOP:
				return;
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep  entrypoint used for managing the views
		 */
		Session_component(Genode::Env &env,
		                  Genode::Ram_allocator &ram,
		                  Window_registry       &window_registry,
		                  Allocator             &session_alloc,
		                  Session_label   const &session_label,
		                  Pointer::Tracker      &pointer_tracker,
		                  Click_handler         &click_handler)
		:
			_env(env),
			_session_label(session_label),
			_ram(ram),
			_window_registry(window_registry),
			_top_level_view_alloc(&session_alloc),
			_child_view_alloc(&session_alloc),
			_input_session_cap(env.ep().manage(_input_session)),
			_click_handler(click_handler),
			_pointer_state(pointer_tracker),
			_view_handle_registry(session_alloc)
		{
			_gui_input.sigh(_input_handler);
			_session.mode_sigh(_mode_handler);
			_input_session.event_queue().enabled(true);
		}

		~Session_component()
		{
			while (Top_level_view *view = _top_level_views.first())
				_destroy_view_object(*view);

			while (Child_view *view = _child_views.first())
				_destroy_view_object(*view);

			_env.ep().dissolve(_input_session);
		}

		using List<Session_component>::Element::next;

		void upgrade(char const *args)
		{
			_session.upgrade(Genode::session_resources_from_args(args));
		}

		void try_to_init_real_child_views()
		{
			for (Child_view *v = _child_views.first(); v; v = v->next())
				v->try_to_init_real_view();
		}

		void update_stacking_order_of_children(Window_registry::Id id)
		{
			for (Child_view *v = _child_views.first(); v; v = v->next())
				if (v->belongs_to_win_id(id))
					v->update_child_stacking();
		}

		void hide_content_child_views(Window_registry::Id id)
		{
			for (Child_view *v = _child_views.first(); v; v = v->next())
				if (v->belongs_to_win_id(id))
					v->hide();
		}

		void content_geometry(Window_registry::Id id, Rect rect)
		{
			for (Top_level_view *v = _top_level_views.first(); v; v = v->next()) {
				if (!v->has_win_id(id))
					continue;

				v->content_geometry(rect);
				break;
			}
		}

		View_capability content_view(Window_registry::Id id)
		{
			for (Top_level_view *v = _top_level_views.first(); v; v = v->next())
				if (v->has_win_id(id.value))
					return v->content_view();

			return View_capability();
		}

		bool has_win_id(unsigned id) const
		{
			for (Top_level_view const *v = _top_level_views.first(); v; v = v->next())
				if (v->has_win_id(id))
					return true;

			return false;
		}

		Session_label session_label() const { return _session_label; }

		bool matches_session_label(char const *selector) const
		{
			using namespace Genode;

			/*
			 * Append label separator to match selectors with a trailing
			 * separator.
			 *
			 * The code snippet originates from nitpicker's 'gui_session.h'.
			 */
			String<Session_label::capacity() + 4> const label(_session_label, " ->");
			return strcmp(label.string(), selector, strlen(selector)) == 0;
		}

		void request_resize(Area size)
		{
			_requested_size   = size;
			_resize_requested = true;

			/* notify client */
			if (_mode_sigh.valid())
				Signal_transmitter(_mode_sigh).submit();
		}

		void hidden(bool hidden)
		{
			for (Top_level_view *v = _top_level_views.first(); v; v = v->next())
				v->hidden(hidden);
		}

		Pointer::Position last_observed_pointer_pos() const
		{
			return _pointer_state.last_observed_pos();
		}

		/**
		 * Return session capability to real GUI session
		 */
		Capability<Session> session() { return _session.rpc_cap(); }


		/***************************
		 ** GUI session interface **
		 ***************************/
		
		Framebuffer::Session_capability framebuffer_session() override
		{
			return _session.framebuffer_session();
		}

		Input::Session_capability input_session() override
		{
			return _input_session_cap;
		}

		View_handle create_view() override
		{
			View &view = _create_view_object();
			_env.ep().manage(view);
			return _view_handle_registry.alloc(view);
		}

		View_handle create_child_view(View_handle parent) override
		{
			try {
				View &view = _create_child_view_object(parent);
				_env.ep().manage(view);
				return _view_handle_registry.alloc(view);
			}
			catch (View_handle_registry::Lookup_failed) {
				return View_handle(); }
		}

		void destroy_view(View_handle handle) override
		{
			try {
				/*
				 * Lookup the view but release the lock to prevent the view
				 * destructor from taking the lock a second time. The locking
				 * aspect of the weak ptr is not needed within the wm because
				 * the component is single-threaded.
				 */
				View *view_ptr = nullptr;
				{
					Locked_ptr<View> view(_view_handle_registry.lookup(handle));
					if (view.valid())
						view_ptr = view.operator->();
				}

				if (view_ptr)
					_destroy_view_object(*view_ptr);

				_view_handle_registry.free(handle);
			} catch (View_handle_registry::Lookup_failed) { }
		}

		View_handle view_handle(View_capability view_cap, View_handle handle) override
		{
			return _env.ep().rpc_ep().apply(view_cap, [&] (View *view) {
				return (view) ? _view_handle_registry.alloc(*view, handle)
				              : View_handle(); });
		}

		View_capability view_capability(View_handle handle) override
		{
			Locked_ptr<View> view(_view_handle_registry.lookup(handle));

			return view.valid() ? view->cap() : View_capability();
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

			/* propagate window-list changes to the layouter */
			_window_registry.flush();
		}

		Framebuffer::Mode mode() override
		{
			Framebuffer::Mode const real_mode = _session.mode();

			/*
			 * While resizing the window, return requested window size as
			 * mode
			 */
			if (_resize_requested)
				return Framebuffer::Mode { .area = _requested_size };

			/*
			 * If the first top-level view has a defined size, use it
			 * as the size of the virtualized GUI session.
			 */
			if (Top_level_view const *v = _top_level_views.first())
				if (v->size().valid())
					return Framebuffer::Mode { .area = v->size() };

			/*
			 * If top-level view has yet been defined, return the real mode.
			 */
			return real_mode;
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			_mode_sigh = sigh;

			/*
			 * We consider a window as resizable if the client shows interest
			 * in mode-change notifications.
			 */
			bool const resizeable = _mode_sigh.valid();

			for (Top_level_view *v = _top_level_views.first(); v; v = v->next())
				v->resizeable(resizeable);
		}

		void buffer(Framebuffer::Mode mode, bool has_alpha) override
		{
			/*
			 * We must not perform the 'buffer' operation on the connection
			 * object because the 'Gui::Connection::buffer' implementation
			 * implicitly performs upgrade operations.
			 *
			 * Here, we merely want to forward the buffer RPC call to the
			 * wrapped GUI session. Otherwise, we would perform session
			 * upgrades initiated by the wm client's buffer operation twice.
			 */
			Gui::Session_client(_env.rm(), _session.cap()).buffer(mode, has_alpha);
			_has_alpha = has_alpha;
		}

		void focus(Genode::Capability<Gui::Session>) override { }
};


class Wm::Gui::Root : public Genode::Rpc_object<Genode::Typed_root<Gui::Session> >,
                      public Decorator_content_callback
{
	private:

		/**
		 * Noncopyable
		 */
		Root(Root const &);
		Root &operator = (Root const &);

		Genode::Env &_env;

		Genode::Attached_rom_dataspace _config { _env, "config" };

		Allocator &_md_alloc;

		Genode::Ram_allocator &_ram;

		enum { STACK_SIZE = 1024*sizeof(long) };

		Pointer::Tracker &_pointer_tracker;

		Reporter &_focus_request_reporter;

		unsigned _focus_request_cnt = 0;

		Window_registry &_window_registry;

		Input::Session_component _window_layouter_input { _env, _env.ram() };

		Input::Session_capability _window_layouter_input_cap { _env.ep().manage(_window_layouter_input) };

		/* handler that forwards clicks into unfocused windows to the layouter */
		struct Click_handler : Gui::Click_handler
		{
			Input::Session_component &window_layouter_input;

			void handle_click(Gui::Point pos) override
			{
				/*
				 * Supply artificial mouse click to the decorator's input session
				 * (which is routed to the layouter).
				 */
				window_layouter_input.submit(Input::Absolute_motion{pos.x, pos.y});
				window_layouter_input.submit(Input::Press{Input::BTN_LEFT});
				window_layouter_input.submit(Input::Release{Input::BTN_LEFT});
			}

			Click_handler(Input::Session_component &window_layouter_input)
			: window_layouter_input(window_layouter_input) { }

		} _click_handler { _window_layouter_input };

		/**
		 * List of regular sessions
		 */
		List<Session_component> _sessions { };

		Layouter_gui_session *_layouter_session = nullptr;

		List<Decorator_gui_session> _decorator_sessions { };

		/**
		 * GUI session used to perform session-control operations
		 */
		Gui::Session &_focus_gui_session;

	public:

		/**
		 * Constructor
		 */
		Root(Genode::Env &env,
		     Window_registry &window_registry, Allocator &md_alloc,
		     Genode::Ram_allocator &ram,
		     Pointer::Tracker &pointer_tracker, Reporter &focus_request_reporter,
		     Gui::Session &focus_gui_session)
		:
			_env(env),
			_md_alloc(md_alloc), _ram(ram),
			_pointer_tracker(pointer_tracker),
			_focus_request_reporter(focus_request_reporter),
			_window_registry(window_registry),
			_focus_gui_session(focus_gui_session)
		{
			_window_layouter_input.event_queue().enabled(true);

			env.parent().announce(env.ep().manage(*this));
		}

		Pointer::Position last_observed_pointer_pos() const
		{
			Pointer::Position pos { };

			for (Decorator_gui_session const *s = _decorator_sessions.first(); s; s = s->next()) {
				if (!pos.valid)
					pos = s->last_observed_pointer_pos(); };

			if (pos.valid)
				return pos;

			for (Session_component const *s = _sessions.first(); s; s = s->next()) {
				if (!pos.valid)
					pos = s->last_observed_pointer_pos(); };

			return pos;
		}


		/********************
		 ** Root interface **
		 ********************/

		Genode::Session_capability session(Session_args const &args,
		                                   Affinity     const &) override
		{
			Genode::Session_label const session_label =
				Genode::label_from_args(args.string());

			enum Role { ROLE_DECORATOR, ROLE_LAYOUTER, ROLE_REGULAR, ROLE_DIRECT };
			Role role = ROLE_REGULAR;

			/*
			 * Determine session policy
			 */
			try {
				Genode::Xml_node policy =
					Genode::Session_policy(session_label, _config.xml());

				auto const value = policy.attribute_value("role", String<16>());

				if (value == "layouter")  role = ROLE_LAYOUTER;
				if (value == "decorator") role = ROLE_DECORATOR;
				if (value == "direct")    role = ROLE_DIRECT;
			}
			catch (...) { }

			switch (role) {

			case ROLE_REGULAR:
				{
					auto session = new (_md_alloc)
						Session_component(_env, _ram, _window_registry,
						                  _md_alloc, session_label,
						                  _pointer_tracker,
						                  _click_handler);
					_sessions.insert(session);
					return _env.ep().manage(*session);
				}

			case ROLE_DECORATOR:
				{
					auto session = new (_md_alloc)
						Decorator_gui_session(_env, _ram, _md_alloc,
						                      _pointer_tracker,
						                      _window_layouter_input,
						                      *this);
					_decorator_sessions.insert(session);
					return _env.ep().manage(*session);
				}

			case ROLE_LAYOUTER:
				{
					_layouter_session = new (_md_alloc)
						Layouter_gui_session(_env, _window_layouter_input_cap);

					return _env.ep().manage(*_layouter_session);
				}

			case ROLE_DIRECT:
				{
					Direct_gui_session *session = new (_md_alloc)
						Direct_gui_session(_env, session_label);

					return _env.ep().manage(*session);
				}
			}

			return { };
		}

		void upgrade(Genode::Session_capability session_cap, Upgrade_args const &args) override
		{
			if (!args.valid_string()) return;

			auto lambda = [&] (Rpc_object_base *session) {
				if (!session) {
					Genode::warning("session lookup failed");
					return;
				}

				Session_component *regular_session =
					dynamic_cast<Session_component *>(session);

				if (regular_session)
					regular_session->upgrade(args.string());

				Decorator_gui_session *decorator_session =
					dynamic_cast<Decorator_gui_session *>(session);

				if (decorator_session)
					decorator_session->upgrade(args.string());

				Direct_gui_session *direct_session =
					dynamic_cast<Direct_gui_session *>(session);

				if (direct_session)
					direct_session->upgrade(args.string());
			};
			_env.ep().rpc_ep().apply(session_cap, lambda);
		}

		void close(Genode::Session_capability session_cap) override
		{
			Genode::Rpc_entrypoint &ep = _env.ep().rpc_ep();

			Session_component *regular_session =
				ep.apply(session_cap, [this] (Session_component *session) {
					if (session) {
						_sessions.remove(session);
						_env.ep().dissolve(*session);
					}
					return session;
				});
			if (regular_session) {
				Genode::destroy(_md_alloc, regular_session);
				return;
			}

			Direct_gui_session *direct_session =
				ep.apply(session_cap, [this] (Direct_gui_session *session) {
					if (session) {
						_env.ep().dissolve(*session);
					}
					return session;
				});
			if (direct_session) {
				Genode::destroy(_md_alloc, direct_session);
				return;
			}

			Decorator_gui_session *decorator_session =
				ep.apply(session_cap, [this] (Decorator_gui_session *session) {
					if (session) {
						_decorator_sessions.remove(session);
						_env.ep().dissolve(*session);
					}
					return session;
				});
			if (decorator_session) {
				Genode::destroy(_md_alloc, decorator_session);
				return;
			}

			auto layouter_lambda = [this] (Layouter_gui_session *session) {
				this->_env.ep().dissolve(*_layouter_session);
				_layouter_session = nullptr;
				return session;
			};

			if (ep.apply(session_cap, layouter_lambda) == _layouter_session) {
				Genode::destroy(_md_alloc, _layouter_session);
				return;
			}
		}


		/******************************************
		 ** Decorator_content_callback interface **
		 ******************************************/

		/*
		 * This function is called once the decorator has produced the content
		 * view for a new window, or when a window is brought to the front.
		 */
		View_capability content_view(Window_registry::Id id) override
		{
			/*
			 * Propagate the request to the sessions. It will be picked up
			 * by the session to which the specified window ID belongs.
			 * The real content view will be created as a side effect of
			 * calling 's->content_view'.
			 */
			for (Session_component *s = _sessions.first(); s; s = s->next())
				if (s->has_win_id(id.value))
					return s->content_view(id.value);

			return View_capability();
		}

		void update_content_child_views(Window_registry::Id id) override
		{
			/*
			 * Try to create physical views for its child views.
			 */
			for (Session_component *s = _sessions.first(); s; s = s->next()) {
				s->try_to_init_real_child_views();
			}

			/*
			 * Apply the stacking order to the child views that belong to the
			 * given window ID. I.e., when the window was brought to the front,
			 * we need to restack its child views such that they end up in
			 * front of the top-level view. Otherwise, the top-level view
			 * will obstruct the child views.
			 */
			for (Session_component *s = _sessions.first(); s; s = s->next())
				s->update_stacking_order_of_children(id);
		}

		void hide_content_child_views(Window_registry::Id id) override
		{
			/*
			 * Destroy physical views for the child views belonging to the
			 * specified id.
			 */
			for (Session_component *s = _sessions.first(); s; s = s->next())
				s->hide_content_child_views(id);
		}

		void content_geometry(Window_registry::Id id, Rect rect) override
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				s->content_geometry(id, rect);
		}

		Capability<Gui::Session> lookup_gui_session(unsigned win_id)
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				if (s->has_win_id(win_id))
					return s->session();

			return { };
		}

		void request_resize(unsigned win_id, Area size)
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				if (s->has_win_id(win_id))
					return s->request_resize(size);
		}
};

#endif /* _GUI_H_ */
