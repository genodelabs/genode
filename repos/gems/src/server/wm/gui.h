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
#include <root/component.h>
#include <gui_session/connection.h>
#include <input_session/capability.h>
#include <input/component.h>
#include <os/dynamic_rom_session.h>

/* local includes */
#include <window_registry.h>
#include <decorator_gui.h>
#include <layouter_gui.h>
#include <direct_gui.h>

namespace Wm { namespace Gui {

	using namespace ::Gui;

	class Click_handler;
	class Input_origin_changed_handler;
	class View;
	class Top_level_view;
	class Child_view;
	class Session_control_fn;
	class Session_component;
	class Root;
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


class Wm::Gui::View : private Weak_object<View>, public Rpc_object< ::Gui::View>
{
	private:

		friend class Weak_ptr<View>;
		friend class Locked_ptr<View>;

	protected:

		using Title   = Gui::Title;
		using Command = Gui::Session::Command;
		using View_id = Gui::View_id;

		Session_label           _session_label;
		Real_gui               &_real_gui;
		Gui::View_ref           _real_view_ref { };
		View_ids::Element const _real_view;
		Title                   _title           { };
		Rect                    _geometry        { };
		Point                   _buffer_offset   { };
		Weak_ptr<View>          _neighbor_ptr    { };
		bool                    _neighbor_behind { };
		bool                    _has_alpha;
		bool                    _layouted = false;

		void _with_temporary_view_id(View_capability cap, auto const &fn)
		{
			Gui::View_ref          ref { };
			Gui::View_ids::Element tmp { ref, _real_gui.view_ids };

			switch (_real_gui.session.associate(tmp.id(), cap)) {
			case Gui::Session::Associate_result::OUT_OF_RAM:
			case Gui::Session::Associate_result::OUT_OF_CAPS:
			case Gui::Session::Associate_result::INVALID:
				warning("unable to obtain view ID for given view capability");
				return;
			case Gui::Session::Associate_result::OK:
				break;
			}
			fn(tmp.id());
			_real_gui.session.release_view_id(tmp.id());
		};

		View(Real_gui            &real_gui,
		     Session_label const &session_label,
		     bool                 has_alpha)
		:
			_session_label(session_label), _real_gui(real_gui),
			_real_view(_real_view_ref, _real_gui.view_ids),
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
			_propagate_view_geometry();
			_real_gui.enqueue<Command::Offset>(_real_view.id(), _buffer_offset);
			_real_gui.enqueue<Command::Title> (_real_view.id(), _title);

			if (neighbor.valid()) {
				_with_temporary_view_id(neighbor->real_view_cap(), [&] (View_id id) {
					if (_neighbor_behind)
						_real_gui.enqueue<Command::Front_of>(_real_view.id(), id);
					else
						_real_gui.enqueue<Command::Behind_of>(_real_view.id(), id);

					_real_gui.execute();
				});

			} else {
				if (_neighbor_behind)
					_real_gui.enqueue<Command::Front>(_real_view.id());
				else
					_real_gui.enqueue<Command::Back>(_real_view.id());

				_real_gui.execute();
			}
		}

		void _apply_view_config()
		{
			Locked_ptr<View> neighbor(_neighbor_ptr);
			_unsynchronized_apply_view_config(neighbor);
		}

	public:

		~View()
		{
			_real_gui.session.destroy_view(_real_view.id());
		}

		using Weak_object<View>::weak_ptr;
		using Weak_object<View>::lock_for_destruction;

		Point virtual_position() const { return _geometry.at; }

		virtual bool belongs_to_win_id(Window_registry::Id id) const = 0;

		virtual void geometry(Rect geometry)
		{
			_geometry = geometry;
			_propagate_view_geometry();
			_real_gui.execute();
		}

		virtual void title(Title const &title)
		{
			_title = title;

			_real_gui.enqueue<Command::Title>(_real_view.id(), title);
			_real_gui.execute();
		}

		virtual Point input_anchor_position() const = 0;

		virtual void stack(Weak_ptr<View>, bool) { }

		View_capability real_view_cap()
		{
			return _real_gui.session.view_capability(_real_view.id()).convert<View_capability>(
				[&] (View_capability cap)   { return cap; },
				[&] (Gui::Session::View_capability_error) {
					warning("real_view_cap unable to obtain view capability");
					return View_capability(); });
		}

		void buffer_offset(Point buffer_offset)
		{
			_buffer_offset = buffer_offset;

			_real_gui.enqueue<Command::Offset>(_real_view.id(), _buffer_offset);
			_real_gui.execute();
		}

		bool has_alpha() const { return _has_alpha; }

		bool layouted() const { return _layouted; }
};


class Wm::Gui::Top_level_view : public View, private List<Top_level_view>::Element
{
	public:

		using View_result = Gui::Session::View_result;

	private:

		friend class List<Top_level_view>;

		Window_registry::Create_result _win_id = Window_registry::Create_error::IDS_EXHAUSTED;

		Window_registry &_window_registry;

		Input_origin_changed_handler &_input_origin_changed_handler;

		/*
		 * Geometry of window-content view, which corresponds to the location
		 * of the window content as known by the decorator.
		 */
		Rect _content_geometry { };

		bool _resizeable = false;

		Title _window_title { };

		Session_label const _session_label;

		View_result _init_real_view()
		{
			return _real_gui.session.view(_real_view.id(), { .title = _title,
			                                                 .rect  = { },
			                                                 .front = false });
		}

		View_result const _real_view_result = _init_real_view();

		using Command = Gui::Session::Command;

		void _with_optional_win_id(auto const &fn) const
		{
			_win_id.with_result([&] (Window_registry::Id id) { fn(id); },
			                    [&] (Window_registry::Create_error) { });
		}

	public:

		Top_level_view(Real_gui                     &real_gui,
		               bool                          has_alpha,
		               Window_registry              &window_registry,
		               Input_origin_changed_handler &input_origin_changed_handler)
		:
			View(real_gui, real_gui.label, has_alpha),
			_window_registry(window_registry),
			_input_origin_changed_handler(input_origin_changed_handler),
			_session_label(real_gui.label)
		{ }

		~Top_level_view()
		{
			_with_optional_win_id([&] (Window_registry::Id id) {
				_window_registry.destroy(id); });

			View::lock_for_destruction();
		}

		View_result real_view_result() const { return _real_view_result; }

		using List<Top_level_view>::Element::next;

		void _propagate_view_geometry() override { }

		void geometry(Rect geometry) override
		{
			/*
			 * Add window to the window-list model on the first call. We
			 * defer the creation of the window ID until the time when the
			 * initial geometry is known.
			 */
			if (!_win_id.ok()) {
				_win_id = _window_registry.create({
					.title      = _window_title,
					.label      = _session_label,
					.area       = geometry.area,
					.alpha      = { View::has_alpha() },
					.hidden     = { },
					.resizeable = { _resizeable }
				});
			} else {
				_with_optional_win_id([&] (Window_registry::Id id) {
					_window_registry.area(id, geometry.area); });
			}

			View::geometry(geometry);
		}

		Area size() const { return _geometry.area; }

		void title(Title const &title) override
		{
			View::title(title);

			_window_title = title;

			_with_optional_win_id([&] (Window_registry::Id id) {
				_window_registry.title(id, _window_title); });
		}

		bool has_win_id(Window_registry::Id id) const
		{
			bool result = false;
			_with_optional_win_id([&] (Window_registry::Id this_id) {
				result = (this_id == id); });
			return result;
		}

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

			_layouted = true;

			if (position_changed)
				_input_origin_changed_handler.input_origin_changed();
		}

		View_capability content_view() { return real_view_cap(); }

		void hidden(bool hidden)
		{
			_with_optional_win_id([&] (Window_registry::Id id) {
				_window_registry.hidden(id, hidden); });
		}

		void resizeable(bool resizeable)
		{
			_resizeable = resizeable;

			_with_optional_win_id([&] (Window_registry::Id id) {
				_window_registry.resizeable(id, resizeable); });
		}
};


class Wm::Gui::Child_view : public View, private List<Child_view>::Element
{
	public:

		using View_result = Gui::Session::Child_view_result;

	private:

		friend class List<Child_view>;

		Weak_ptr<View> mutable _parent;

		bool _visible = false;

		View_result _real_view_result = try_to_init_real_view();

	public:

		Child_view(Real_gui      &real_gui,
		           bool           has_alpha,
		           Weak_ptr<View> parent)
		:
			View(real_gui, real_gui.label, has_alpha), _parent(parent)
		{ }

		~Child_view()
		{
			View::lock_for_destruction();
		}

		View_result real_view_result() const { return _real_view_result; }

		using List<Child_view>::Element::next;

		void _propagate_view_geometry() override
		{
			_real_gui.enqueue<Command::Geometry>(_real_view.id(), _geometry);
		}

		void stack(Weak_ptr<View> neighbor_ptr, bool behind) override
		{
			_neighbor_ptr    = neighbor_ptr;
			_neighbor_behind = behind;

			auto parent_position_known = [&]
			{
				Locked_ptr<View> parent(_parent);
				return parent.valid() && parent->layouted();
			};

			if (parent_position_known())
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

		View_result try_to_init_real_view()
		{
			using Child_view_result = Gui::Session::Child_view_result;

			Child_view_result result = Child_view_result::INVALID;

			Locked_ptr<View> parent(_parent);
			if (!parent.valid())
				return result;

			_with_temporary_view_id(parent->real_view_cap(), [&] (View_id parent_id) {

				if (_visible)
					return;

				Gui::Session::View_attr const attr { .title = _title,
				                                     .rect  = _geometry,
				                                     .front = false };

				result = _real_gui.session.child_view(_real_view.id(), parent_id, attr);

				if (result != Child_view_result::OK) {
					warning("unable to create child view");
					return;
				}

				_visible = true;
			});

			if (parent->layouted()) {
				if (_neighbor_ptr == _parent)
					_unsynchronized_apply_view_config(parent);
				else
					_apply_view_config();
			}

			return result;
		}

		void update_child_stacking()
		{
			_apply_view_config();
		}

		void hide()
		{
			_real_gui.session.destroy_view(_real_view.id());
			_visible = false;
		}
};


class Wm::Gui::Session_component : public Session_object<Gui::Session>,
                                   private List<Session_component>::Element,
                                   private Input_origin_changed_handler,
                                   private Upgradeable,
                                   private Dynamic_rom_session::Xml_producer,
                                   private Input::Session_component::Action
{
	public:

		struct Action : Interface
		{
			virtual void gen_screen_area_info(Xml_generator &) const = 0;
		};

	private:

		friend class List<Session_component>;

		using View_id = Gui::View_id;

		struct View_ref : Gui::View_ref
		{
			Weak_ptr<View> _weak_ptr;

			View_ids::Element id;

			View_ref(Weak_ptr<View> view, View_ids &ids)
			: _weak_ptr(view), id(*this, ids) { }

			View_ref(Weak_ptr<View> view, View_ids &ids, View_id id)
			: _weak_ptr(view), id(*this, ids, id) { }

			auto with_view(auto const &fn, auto const &missing_fn) -> decltype(missing_fn())
			{
				/*
				 * Release the lock before calling 'fn' to allow the nesting of
				 * 'with_view' calls. The locking aspect of the weak ptr is not
				 * needed here because the component is single-threaded.
				 */
				View *ptr = nullptr;
				{
					Locked_ptr<View> view(_weak_ptr);
					if (view.valid())
						ptr = view.operator->();
				}
				return ptr ? fn(*ptr) : missing_fn();
			}
		};

		Env    &_env;
		Action &_action;

		Accounted_ram_allocator _ram {
			_env.ram(), _ram_quota_guard(), _cap_quota_guard() };

		Sliced_heap                  _session_alloc { _ram, _env.rm() };
		Real_gui                     _real_gui { _env, _label };
		Window_registry             &_window_registry;
		Slab<Top_level_view, 8000>   _top_level_view_alloc { _session_alloc };
		Slab<Child_view, 7000>       _child_view_alloc     { _session_alloc };
		Slab<View_ref, 4000>         _view_ref_alloc       { _session_alloc };
		List<Top_level_view>         _top_level_views { };
		List<Child_view>             _child_views { };
		View_ids                     _view_ids { };

		Input::Session_component _input_session {
			_env.ep(), _ram, _env.rm(), *this };

		bool _exclusive_input_requested = false,
		     _exclusive_input_granted   = false;

		/* used for hiding the click-to-grab event from the client */
		bool _consume_one_btn_left_release = false;

		/**
		 * Input::Session_component::Action interface
		 */
		void exclusive_input_requested(bool const requested) override
		{
			if (requested == _exclusive_input_requested)
				return;

			/*
			 * Allow immediate changes when
			 *
			 * 1. Exclusive input is already granted by the user having clicked
			 *    into the window, or
			 * 2. The client yields the exclusivity, or
			 * 3. Transient exclusive input is requested while a button is held.
			 *    In this case, exclusive input will be revoked as soon as the
			 *    last button/key is released.
			 */
			if (_exclusive_input_granted || _key_cnt || !requested)
				_gui_input.exclusive(requested);

			_exclusive_input_requested = requested;
		}

		Click_handler &_click_handler;

		struct Info_rom_session : Dynamic_rom_session
		{
			Session_component &_session;

			Info_rom_session(Session_component &session, auto &&... args)
			: Dynamic_rom_session(args...), _session(session) { }

			void sigh(Signal_context_capability sigh) override
			{
				Dynamic_rom_session::sigh(sigh);

				/*
				 * We consider a window as resizable if the client shows interest
				 * in mode-change notifications.
				 */
				_session._resizeable = sigh.valid();
				for (Top_level_view *v = _session._top_level_views.first(); v; v = v->next())
					v->resizeable(_session._resizeable);
			}
		};

		Constructible<Info_rom_session> _info_rom { };

		bool           _resizeable = false;
		Area           _requested_size { };
		bool           _resize_requested = false;
		bool           _close_requested = false;
		bool           _has_alpha = false;
		Pointer::State _pointer_state;
		Point    const _initial_pointer_pos { -1, -1 };
		Point          _pointer_pos = _initial_pointer_pos;
		Point          _virtual_pointer_pos { };
		unsigned       _key_cnt = 0;

		auto _with_view(View_id id, auto const &fn, auto const &missing_fn)
		-> decltype(missing_fn())
		{
			return _view_ids.apply<View_ref>(id,
				[&] (View_ref &view_ref) {
					return view_ref.with_view(
						[&] (View &view)        { return fn(view); },
						[&] /* view vanished */ { return missing_fn(); });
				},
				[&] /* ID does not exist */ { return missing_fn(); });
		}

		/*
		 * Command buffer
		 */
		using Command_buffer = Gui::Session::Command_buffer;

		Attached_ram_dataspace _command_ds { _ram, _env.rm(), sizeof(Command_buffer) };

		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		/*
		 * Input
		 */
		Input::Session_client _gui_input    { _env.rm(), _real_gui.session.input() };
		Attached_dataspace    _gui_input_ds { _env.rm(), _gui_input.dataspace() };

		Signal_handler<Session_component> _input_handler {
			_env.ep(), *this, &Session_component::_handle_input };

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

					/*
					 * Handle pointer grabbing/ungrabbing
					 */

					/* revoke transient exclusive input (while clicked) */
					if (ev.release() && _key_cnt == 0)
						if (_exclusive_input_requested && !_exclusive_input_granted)
							_gui_input.exclusive(false);

					/* grant exclusive input when clicking into window */
					if (ev.key_press(Input::BTN_LEFT) && _key_cnt == 1) {
						if (_exclusive_input_requested && !_exclusive_input_granted) {
							_gui_input.exclusive(true);
							_exclusive_input_granted = true;
							_consume_one_btn_left_release = true;
							continue;
						}
					}
					if (ev.key_release(Input::BTN_LEFT)) {
						if (_consume_one_btn_left_release) {
							_consume_one_btn_left_release = false;
							continue;
						}
					}

					/* submit event to the client */
					_input_session.submit(_translate_event(ev, input_origin));
				}
			}
		}

		/**
		 * Dynamic_rom_session::Xml_producer interface
		 */
		void produce_xml(Xml_generator &xml) override
		{
			_action.gen_screen_area_info(xml);

			if (_close_requested) {
				xml.node("capture", [&] { xml.attribute("closed", "yes"); });
				return;
			}

			auto virtual_capture_area = [&]
			{
				/*
				 * While resizing the window, return requested window size as
				 * mode
				 */
				if (_resize_requested)
					return _requested_size;

				/*
				 * If the first top-level view has a defined size, use it
				 * as the size of the virtualized GUI session.
				 */
				if (Top_level_view const *v = _top_level_views.first())
					if (v->size().valid())
						return v->size();

				return Area { };
			};

			auto gen_attr = [&] (Area const area)
			{
				if (area.valid()) {
					xml.attribute("width",  area.w);
					xml.attribute("height", area.h);
				}
			};

			xml.node("capture", [&] { gen_attr(virtual_capture_area()); });
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

		void _dissolve_view_from_ep(View &view)
		{
			if (view.cap().valid()) {
				_env.ep().dissolve(view);
				replenish(Cap_quota { 1 });
			}
		}

		void _destroy_top_level_view(Top_level_view &view)
		{
			_top_level_views.remove(&view);
			_dissolve_view_from_ep(view);
			destroy(&_top_level_view_alloc, &view);
		}

		void _destroy_child_view(Child_view &view)
		{
			_child_views.remove(&view);
			_dissolve_view_from_ep(view);
			destroy(&_child_view_alloc, &view);
		}

		void _execute_command(Command const &command)
		{
			auto with_this = [&] (auto const &args, auto const &fn)
			{
				Session_component::_with_view(args.view,
					[&] (View &view) { fn(view, args); },
					[&] /* ignore operations on non-existing views */ { });
			};

			switch (command.opcode) {

			case Command::GEOMETRY:

				with_this(command.geometry, [&] (View &view, Command::Geometry const &args) {
					view.geometry(args.rect); });
				return;

			case Command::OFFSET:

				with_this(command.offset, [&] (View &view, Command::Offset const &args) {
					view.buffer_offset(args.offset); });
				return;

			case Command::FRONT:

				with_this(command.front, [&] (View &view, auto const &) {
					view.stack(Weak_ptr<View>(), true); });
				return;

			case Command::FRONT_OF:

				with_this(command.front_of, [&] (View &view, Command::Front_of const &args) {
					Session_component::_with_view(args.neighbor,
						[&] (View &neighbor) {
							if (&view != &neighbor)
								view.stack(neighbor.weak_ptr(), true); },
						[&] { });
				});
				return;

			case Command::TITLE:

				with_this(command.title, [&] (View &view, Command::Title const &args) {

					char sanitized_title[args.title.capacity()];

					copy_cstring(sanitized_title, command.title.title.string(),
					             sizeof(sanitized_title));

					for (char *c = sanitized_title; *c; c++)
						if (*c == '"')
							*c = '\'';

					view.title(sanitized_title);
				});
				return;

			case Command::BACK:
			case Command::BEHIND_OF:
			case Command::BACKGROUND:
			case Command::NOP:
				return;
			}
		}

	public:

		Session_component(Env              &env,
		                  Action           &action,
		                  Resources  const &resources,
		                  Label      const &label,
		                  Diag       const  diag,
		                  Window_registry  &window_registry,
		                  Pointer::Tracker &pointer_tracker,
		                  Click_handler    &click_handler)
		:
			Session_object<Gui::Session>(env.ep(), resources, label, diag),
			Xml_producer("panorama"),
			_env(env), _action(action),
			_window_registry(window_registry),
			_click_handler(click_handler),
			_pointer_state(pointer_tracker)
		{
			_gui_input.sigh(_input_handler);
			_input_session.event_queue().enabled(true);
		}

		~Session_component()
		{
			while (_view_ids.apply_any<View_ref>([&] (View_ref &view_ref) {
				destroy(_view_ref_alloc, &view_ref); }));

			while (Top_level_view *view = _top_level_views.first())
				_destroy_top_level_view(*view);

			while (Child_view *view = _child_views.first())
				_destroy_child_view(*view);
		}

		using List<Session_component>::Element::next;

		void upgrade_local_or_remote(Resources const &resources)
		{
			_upgrade_local_or_remote(resources, *this, _real_gui);
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
				if (v->has_win_id(id))
					return v->content_view();

			return View_capability();
		}

		bool has_win_id(Window_registry::Id id) const
		{
			for (Top_level_view const *v = _top_level_views.first(); v; v = v->next())
				if (v->has_win_id(id))
					return true;

			return false;
		}

		bool matches_session_label(char const *selector) const
		{
			using namespace Genode;

			/*
			 * Append label separator to match selectors with a trailing
			 * separator.
			 *
			 * The code snippet originates from nitpicker's 'gui_session.h'.
			 */
			String<Session_label::capacity() + 4> const label(_label, " ->");
			return strcmp(label.string(), selector, strlen(selector)) == 0;
		}

		void request_resize(Area size)
		{
			_requested_size   = size;
			_resize_requested = true;

			if (_requested_size.count() == 0)
				_close_requested = true;

			/* notify client */
			if (_info_rom.constructed())
				_info_rom->trigger_update();
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
		Capability<Session> session() { return _real_gui.connection.cap(); }

		void propagate_mode_change()
		{
			if (_info_rom.constructed())
				_info_rom->trigger_update();
		}

		void revoke_exclusive_input()
		{
			if (_exclusive_input_granted) {
				_gui_input.exclusive(false);
				_exclusive_input_granted = false;
			}
		}


		/***************************
		 ** GUI session interface **
		 ***************************/
		
		Framebuffer::Session_capability framebuffer() override
		{
			return _real_gui.session.framebuffer();
		}

		Input::Session_capability input() override
		{
			return _input_session.cap();
		}

		template <typename VIEW>
		VIEW::View_result _create_view_with_id(auto &dealloc, View_id id, View_attr const &attr, auto const &create_fn)
		{
			using Result = VIEW::View_result;

			/* precondition for obtaining 'real_view_cap' */
			if (!try_withdraw(Cap_quota { 1 })) {
				_starved_for_caps = true;
				return Result::OUT_OF_CAPS;
			}

			release_view_id(id);

			Result error { };

			VIEW *view_ptr = nullptr;
			try {
				view_ptr = &create_fn();
			}
			catch (Out_of_ram)  {
				_starved_for_ram = true;
				error = Result::OUT_OF_RAM;
			}
			catch (Out_of_caps) {
				_starved_for_caps = true;
				error = Result::OUT_OF_CAPS;
			}
			if (!view_ptr)
				return error;

			/* _real_gui view creation may return OUT_OF_RAM or OUT_OF_CAPS */
			if (view_ptr->real_view_result() != Result::OK) {
				error = view_ptr->real_view_result();
				destroy(dealloc, view_ptr);
				return error;
			}

			View_ref *view_ref_ptr = nullptr;
			try {
				view_ref_ptr =
					new (_view_ref_alloc) View_ref(view_ptr->weak_ptr(), _view_ids, id);
			}
			catch (Out_of_ram)  {
				_starved_for_ram = true;
				error = Result::OUT_OF_RAM;
			}
			catch (Out_of_caps) {
				_starved_for_caps = true;
				error = Result::OUT_OF_CAPS;
			}
			if (!view_ref_ptr) {
				destroy(dealloc, view_ptr);
				return error;
			}

			/* apply initial view attributes */
			_execute_command(Command::Title    { id, attr.title });
			_execute_command(Command::Geometry { id, attr.rect  });
			if (attr.front) {
				_execute_command(Command::Front { id });
				_window_registry.flush();
			}

			return Result::OK;
		}

		View_result view(View_id id, View_attr const &attr) override
		{
			Top_level_view *view_ptr = nullptr;

			View_result const result =
				_create_view_with_id<Top_level_view>(_top_level_view_alloc, id, attr,
					[&] () -> Top_level_view & {
						view_ptr = new (_top_level_view_alloc)
							Top_level_view(_real_gui, _has_alpha,
							               _window_registry, *this);
						return *view_ptr;
					});

			if (result == View_result::OK && view_ptr) {
				view_ptr->resizeable(_resizeable);
				_top_level_views.insert(view_ptr);
			}
			return result;
		}

		Child_view_result child_view(View_id const id, View_id const parent, View_attr const &attr) override
		{
			return _with_view(parent,
				[&] (View &parent) -> Child_view_result {

					Child_view *view_ptr = nullptr;

					Child_view_result const result =
						_create_view_with_id<Child_view>(_child_view_alloc, id, attr,
							[&] () -> Child_view & {
								view_ptr = new (_child_view_alloc)
									Child_view(_real_gui, _has_alpha, parent.weak_ptr());
								return *view_ptr;
							});

					if (result == Child_view_result::OK && view_ptr)
						_child_views.insert(view_ptr);

					return result;
				},
				[&] () -> Child_view_result { return Child_view_result::INVALID; });
		}

		void destroy_view(View_id id) override
		{
			_with_view(id,
				[&] (View &view) {
					for (Child_view *v = _child_views.first(); v; v = v->next())
						if (&view == v) {
							_destroy_child_view(*v);
							replenish(Cap_quota { 1 });
							return;
						}
					for (Top_level_view *v = _top_level_views.first(); v; v = v->next())
						if (&view == v) {
							_destroy_top_level_view(*v);
							replenish(Cap_quota { 1 });
							return;
						}
				},
				[&] /* ID exists but view vanished */ { }
			);
			release_view_id(id);
		}

		Associate_result associate(View_id id, View_capability view_cap) override
		{
			/* prevent ID conflict in 'View_ids::Element' constructor */
			release_view_id(id);

			return _env.ep().rpc_ep().apply(view_cap,
				[&] (View *view_ptr) -> Associate_result {
					if (!view_ptr)
						return Associate_result::INVALID;
					try {
						new (_view_ref_alloc) View_ref(view_ptr->weak_ptr(), _view_ids, id);
						return Associate_result::OK;
					}
					catch (Out_of_ram)  {
						_starved_for_ram = true;
						return Associate_result::OUT_OF_RAM;
					}
					catch (Out_of_caps) {
						_starved_for_caps = true;
						return Associate_result::OUT_OF_CAPS;
					}
				});
		}

		View_capability_result view_capability(View_id id) override
		{
			return _with_view(id,
				[&] (View &view) -> View_capability_result
				{
					if (!view.cap().valid()) {
						if (!try_withdraw(Cap_quota { 1 })) {
							_starved_for_caps = true;
							return View_capability_error::OUT_OF_CAPS;
						}
						_env.ep().manage(view);
					}
					return view.cap();
				},
				[&] () -> View_capability_result { return View_capability(); });
		}

		void release_view_id(View_id id) override
		{
			_view_ids.apply<View_ref>(id,
				[&] (View_ref &view_ref) { destroy(_view_ref_alloc, &view_ref); },
				[&] { });
		}

		Dataspace_capability command_dataspace() override
		{
			return _command_ds.cap();
		}

		void execute() override
		{
			for (unsigned i = 0; i < _command_buffer.num(); i++)
				_execute_command(_command_buffer.get(i));

			/* propagate window-list changes to the layouter */
			_window_registry.flush();
		}

		Info_result info() override
		{
			if (!_info_rom.constructed()) {
				Cap_quota const needed_caps { 2 };
				if (!_cap_quota_guard().have_avail(needed_caps)) {
					_starved_for_caps = true;
					return Info_error::OUT_OF_CAPS;
				}

				Ram_quota const needed_ram { 8*1024 };
				if (!_ram_quota_guard().have_avail(needed_ram)) {
					_starved_for_ram = true;
					return Info_error::OUT_OF_RAM;
				}

				try {
					Dynamic_rom_session::Content_producer &rom_producer = *this;
					_info_rom.construct(*this, _env.ep(), _ram, _env.rm(), rom_producer);
					_info_rom->dataspace(); /* eagerly consume RAM and caps */
				}
				catch (Out_of_ram)  { _starved_for_ram  = true; }
				catch (Out_of_caps) { _starved_for_caps = true; }

				if (_starved_for_ram || _starved_for_caps) {
					if (_starved_for_ram)  return Info_error::OUT_OF_RAM;
					if (_starved_for_caps) return Info_error::OUT_OF_CAPS;
				}
			}
			return _info_rom->cap();
		}

		Buffer_result buffer(Framebuffer::Mode mode) override
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
			_has_alpha = mode.alpha;

			Buffer_result const result = _real_gui.session.buffer(mode);

			_window_registry.flush();
			return result;
		}

		void focus(Capability<Gui::Session>) override { }
};


class Wm::Gui::Root : public  Rpc_object<Typed_root<Gui::Session> >,
                      public  Decorator_content_callback,
                      private Input::Session_component::Action
{
	private:

		/**
		 * Noncopyable
		 */
		Root(Root const &);
		Root &operator = (Root const &);

		Env &_env;

		Session_component::Action &_action;

		Attached_rom_dataspace _config { _env, "config" };

		Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

		enum { STACK_SIZE = 1024*sizeof(long) };

		Pointer::Tracker &_pointer_tracker;

		Window_registry &_window_registry;

		Input::Session_component _window_layouter_input {
			_env.ep(), _env.ram(), _env.rm(), *this };

		/**
		 * Input::Session_component::Action interface
		 */
		void exclusive_input_requested(bool) override { }

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
		Gui::Connection &_focus_gui_session;

	public:

		/**
		 * Constructor
		 */
		Root(Env &env, Session_component::Action &action,
		     Window_registry  &window_registry,
		     Pointer::Tracker &pointer_tracker,
		     Gui::Connection  &focus_gui_session)
		:
			_env(env), _action(action),
			_pointer_tracker(pointer_tracker),
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

		static_assert(Gui::Session::CAP_QUOTA == 9);

		Root::Result session(Session_args const &args, Affinity const &) override
		{
			using Session = Genode::Session;

			Session::Label     label     = label_from_args(args.string());
			Session::Resources resources = session_resources_from_args(args.string());
			Session::Diag      diag      = session_diag_from_args(args.string());

			enum Role { ROLE_DECORATOR, ROLE_LAYOUTER, ROLE_REGULAR, ROLE_DIRECT };
			Role role = ROLE_REGULAR;

			/*
			 * Determine session policy
			 */
			with_matching_policy(label, _config.xml(),

				[&] (Xml_node const &policy) {

					auto const value = policy.attribute_value("role", String<16>());

					if (value == "layouter")  role = ROLE_LAYOUTER;
					if (value == "decorator") role = ROLE_DECORATOR;
					if (value == "direct")    role = ROLE_DIRECT;
				},
				[&] { }
			);

			if (role == ROLE_REGULAR || role == ROLE_DECORATOR) {

				size_t const needed_ram = Real_gui::RAM_QUOTA
				                        + sizeof(Session_component)
				                        + _sliced_heap.overhead(sizeof(Session_component))
				                        + 8*1024;

				if (resources.ram_quota.value < needed_ram)
					throw Insufficient_ram_quota();
				resources.ram_quota.value -= needed_ram;

				static constexpr unsigned needed_caps =
					1 +  /* Sliced_heap alloc of Session_component           */
					1 +  /* Session_component RPC cap                        */
					9 +  /* Wrapped nitpicker GUI session  (_real_gui)       */
					1 +  /* Input_session RPC cap          (_input_session)  */
					1 +  /* Input_session events dataspace (_input_session)  */
					1 +  /* Command buffer                 (_command_buffer) */
					1 +  /* Input signal handler           (_input_handler)  */
					1;   /* Content-view capability                          */

				if (resources.cap_quota.value < needed_caps)
					throw Insufficient_cap_quota();
				/* preserve caps for content_view and command buffer ds */
				resources.cap_quota.value -= needed_caps - 2;
			}

			switch (role) {

			case ROLE_REGULAR:
				try {
					Session_component &session = *new (_sliced_heap)
						Session_component(_env, _action, resources, label, diag,
						                  _window_registry,
						                  _pointer_tracker,
						                  _click_handler);
					_sessions.insert(&session);
					return { session.cap() };
				}
				catch (Out_of_ram)  { return Service::Create_error::INSUFFICIENT_RAM; }
				catch (Out_of_caps) { return Service::Create_error::INSUFFICIENT_CAPS; }

			case ROLE_DECORATOR:
				try {
					Decorator_gui_session &session = *new (_sliced_heap)
						Decorator_gui_session(_env, resources, label, diag,
						                      _pointer_tracker,
						                      _window_layouter_input,
						                      *this);
					_decorator_sessions.insert(&session);
					return { session.cap() };
				}
				catch (Out_of_ram)  { return Service::Create_error::INSUFFICIENT_RAM; }
				catch (Out_of_caps) { return Service::Create_error::INSUFFICIENT_CAPS; }

			case ROLE_LAYOUTER:
				try {
					_layouter_session = new (_sliced_heap)
						Layouter_gui_session(_env, resources, label, diag,
						                     _window_layouter_input.cap());

					return { _layouter_session->cap() };
				}
				catch (Out_of_ram)  { return Service::Create_error::INSUFFICIENT_RAM; }
				catch (Out_of_caps) { return Service::Create_error::INSUFFICIENT_CAPS; }

			case ROLE_DIRECT:
				try {
					Direct_gui_session &session = *new (_sliced_heap)
						Direct_gui_session(_env, resources, label, diag);

					return { session.cap() };
				}
				catch (Out_of_ram)  { return Service::Create_error::INSUFFICIENT_RAM; }
				catch (Out_of_caps) { return Service::Create_error::INSUFFICIENT_CAPS; }
			}
			return Service::Create_error::DENIED;
		}

		void upgrade(Genode::Session_capability session_cap, Upgrade_args const &args) override
		{
			if (!args.valid_string()) return;

			auto lambda = [&] (Rpc_object_base *session) {
				if (!session) {
					warning("session lookup failed");
					return;
				}

				Session_component *regular_session =
					dynamic_cast<Session_component *>(session);

				if (regular_session)
					regular_session->upgrade_local_or_remote(session_resources_from_args(args.string()));

				Decorator_gui_session *decorator_session =
					dynamic_cast<Decorator_gui_session *>(session);

				if (decorator_session)
					decorator_session->upgrade_local_or_remote(session_resources_from_args(args.string()));

				Direct_gui_session *direct_session =
					dynamic_cast<Direct_gui_session *>(session);

				if (direct_session)
					direct_session->upgrade(args.string());
			};
			_env.ep().rpc_ep().apply(session_cap, lambda);
		}

		void close(Genode::Session_capability session_cap) override
		{
			Rpc_entrypoint &ep = _env.ep().rpc_ep();

			Session_component *regular_session =
				ep.apply(session_cap, [this] (Session_component *session) {
					if (session)
						_sessions.remove(session);
					return session;
				});
			if (regular_session) {
				destroy(_sliced_heap, regular_session);
				return;
			}

			Direct_gui_session *direct_session =
				ep.apply(session_cap, [this] (Direct_gui_session *session) {
					return session;
				});
			if (direct_session) {
				destroy(_sliced_heap, direct_session);
				return;
			}

			Decorator_gui_session *decorator_session =
				ep.apply(session_cap, [this] (Decorator_gui_session *session) {
					if (session)
						_decorator_sessions.remove(session);
					return session;
				});
			if (decorator_session) {
				destroy(_sliced_heap, decorator_session);
				return;
			}

			auto layouter_lambda = [this] (Layouter_gui_session *session) {
				_layouter_session = nullptr;
				return session;
			};

			if (ep.apply(session_cap, layouter_lambda) == _layouter_session) {
				destroy(_sliced_heap, _layouter_session);
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
				if (s->has_win_id(id))
					return s->content_view(id);

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

		void with_gui_session(Window_registry::Id id, auto const &fn)
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				if (s->has_win_id(id)) {
					fn(s->session());
					return;
				}
		}

		void request_resize(Window_registry::Id win_id, Area size)
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				if (s->has_win_id(win_id))
					return s->request_resize(size);
		}

		void propagate_mode_change()
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				s->propagate_mode_change();
		}

		void revoke_exclusive_input()
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				s->revoke_exclusive_input();
		}
};

#endif /* _GUI_H_ */
