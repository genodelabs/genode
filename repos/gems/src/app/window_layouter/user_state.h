/*
 * \brief  Window layouter
 * \author Norman Feske
 * \date   2013-02-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _USER_STATE_H_
#define _USER_STATE_H_

/* local includes */
#include <key_sequence_tracker.h>

namespace Window_layouter { class User_state; }


class Window_layouter::User_state
{
	public:

		struct Action : Interface
		{
			virtual bool visible(Window_id) = 0;
			virtual void close(Window_id) = 0;
			virtual void toggle_fullscreen(Window_id) = 0;
			virtual void focus(Window_id) = 0;
			virtual void release_grab() = 0;
			virtual void to_front(Window_id) = 0;
			virtual void drag(Window_id, Window::Element, Point clicked, Point curr) = 0;
			virtual void finalize_drag(Window_id, Window::Element, Point clicked, Point final) = 0;
			virtual void pick_up(Window_id) = 0;
			virtual void place_down() = 0;
			virtual void screen(Target::Name const &) = 0;
			virtual void free_arrange_hover_changed() = 0;
			virtual Window::Element free_arrange_element_at(Window_id, Point) = 0;
		};

		struct Hover_state
		{
			Window_id       window_id;
			Window::Element element;

			bool operator != (Hover_state const &other) const
			{
				return window_id != other.window_id
				    || element   != other.element;
			}
		};

	private:

		Action &_action;

		Window_id _hovered_window_id { };
		Window_id _focused_window_id { };
		Window_id _dragged_window_id { };

		unsigned _key_cnt = 0;

		Key_sequence_tracker _key_sequence_tracker { };

		Window::Element _strict_hovered_element { };  /* hovered window control */
		Window::Element _free_hovered_element   { };  /* hovered window area */
		Window::Element _dragged_element        { };

		/*
		 * True while drag operation in progress
		 */
		bool _drag_state = false;

		/*
		 * False if the hover state (hovered window and element) was not known
		 * at the initial click of a drag operation. In this case, the drag
		 * operation is initiated as soon as the hover state becomes known.
		 */
		bool _drag_init_done = false;

		bool _picked_up = false;

		/*
		 * If true, the window element is determined by the sole relation of
		 * the pointer position to the window area, ignoring window controls.
		 */
		bool _free_arrange = false;

		Window::Element _hovered_element() const
		{
			return _free_arrange ? _free_hovered_element : _strict_hovered_element;
		}

		/*
		 * Pointer position at the beginning of a drag operation
		 */
		Point _pointer_clicked { };

		/*
		 * Current pointer position
		 */
		Point _pointer_curr { };

		Focus_history &_focus_history;

		/*
		 * Return true if event is potentially part of a key sequence
		 */
		bool _key(Input::Event const &ev) const { return ev.press() || ev.release(); }

		inline void _handle_event(Input::Event const &, Xml_node);

		void _initiate_drag(Window_id       hovered_window_id,
		                    Window::Element hovered_element)
		{
			/*
			 * This function must never be called without the hover state to be
			 * defined. This assertion checks this precondition.
			 */
			if (!hovered_window_id.valid()) {
				struct Drag_with_undefined_hover_state { };
				throw  Drag_with_undefined_hover_state();
			}

			_drag_init_done    = true;
			_dragged_window_id = hovered_window_id;
			_dragged_element   = hovered_element;

			/*
			 * Toggle maximized (fullscreen) state
			 */
			if (_strict_hovered_element.maximizer()) {

				_dragged_window_id = _hovered_window_id;
				_focused_window_id = _hovered_window_id;
				_focus_history.focus(_focused_window_id);

				_action.toggle_fullscreen(_hovered_window_id);

				_strict_hovered_element = { };
				_hovered_window_id      = { };
				return;
			}

			/*
			 * Bring hovered window to front when clicked
			 */
			if (_focused_window_id != _hovered_window_id) {

				_focused_window_id = _hovered_window_id;
				_focus_history.focus(_focused_window_id);

				_action.to_front(_hovered_window_id);
				_action.focus(_hovered_window_id);
			}

			_action.drag(_dragged_window_id, _dragged_element,
			             _pointer_clicked, _pointer_curr);
		}

		void _update_free_hovered_element()
		{
			_free_hovered_element = { };
			if (_hovered_window_id.valid())
				_free_hovered_element = _action.free_arrange_element_at(_hovered_window_id,
				                                                        _pointer_curr);
		}

	public:

		User_state(Action &action, Focus_history &focus_history)
		:
			_action(action), _focus_history(focus_history)
		{ }

		void handle_input(Input::Event const events[], unsigned num_events,
		                  Xml_node const &config)
		{
			Point const pointer_last = _pointer_curr;

			for (size_t i = 0; i < num_events; i++)
				_handle_event(events[i], config);

			/*
			 * Issue drag operation when in dragged state
			 */
			if (_drag_state && _drag_init_done && (_pointer_curr != pointer_last))
				_action.drag(_dragged_window_id, _dragged_element,
				             _pointer_clicked, _pointer_curr);
		}

		void hover(Window_id window_id, Window::Element element)
		{
			Window_id const orig_hovered_window_id = _hovered_window_id;

			_hovered_window_id      = window_id;
			_strict_hovered_element = element;

			_update_free_hovered_element();

			/*
			 * Check if we have just received an update while already being in
			 * dragged state.
			 *
			 * This can happen when the user selects a new nitpicker domain by
			 * clicking on a window decoration. Prior the click, the new
			 * session is not aware of the current mouse position. So the hover
			 * model is not up to date. As soon as nitpicker assigns the focus
			 * to the new session and delivers the corresponding press event,
			 * we enter the drag state (in the 'handle_input' function. But we
			 * don't know which window is dragged until the decorator updates
			 * the hover model. Now, when the model is updated and we are still
			 * in dragged state, we can finally initiate the window-drag
			 * operation for the now-known window.
			 */
			if (_drag_state && !_drag_init_done && _hovered_window_id.valid())
				_initiate_drag(_hovered_window_id, _strict_hovered_element);

			/*
			 * Let focus follows the pointer, except while dragging or when
			 * the focused window is currently picked up.
			 */
			if (!_drag_state && !_picked_up && _hovered_window_id.valid()
			 && _hovered_window_id != orig_hovered_window_id) {

				_focused_window_id = _hovered_window_id;
				_focus_history.focus(_focused_window_id);
				_action.focus(_focused_window_id);
			}
		}

		void reset_hover()
		{
			/* ignore hover resets when in drag state */
			if (_drag_state)
				return;

			_strict_hovered_element = { };
			_hovered_window_id      = { };
		}

		Window_id focused_window_id() const { return _focused_window_id; }

		void focused_window_id(Window_id id) { _focused_window_id = id; }

		Hover_state hover_state() const
		{
			return { .window_id = _hovered_window_id,
			         .element   = _hovered_element() };
		}
};


void Window_layouter::User_state::_handle_event(Input::Event const &e,
                                                Xml_node config)
{
	Point const orig_pointer_curr = _pointer_curr;
	bool  const orig_free_arrange = _free_arrange;

	e.handle_absolute_motion([&] (int x, int y) {
		_pointer_curr = Point(x, y); });

	if (e.absolute_motion() || e.focus_enter()) {

		if (_drag_state && _drag_init_done)
			_action.drag(_dragged_window_id, _dragged_element,
			             _pointer_clicked, _pointer_curr);
	}

	/* track number of pressed buttons/keys */
	if (e.press())   _key_cnt++;
	if (e.release()) _key_cnt--;

	/* handle key sequences */
	if (_key(e)) {

		if (e.press() && _key_cnt == 1)
			_key_sequence_tracker.reset();

		auto visible = [&] (Window_id id) { return _action.visible(id); };

		_key_sequence_tracker.apply(e, config, [&] (Command const &command) {

			switch (command.type) {

			case Command::TOGGLE_FULLSCREEN:
				_action.toggle_fullscreen(_focused_window_id);
				return;

			case Command::RAISE_WINDOW:
				_action.to_front(_focused_window_id);
				return;

			case Command::NEXT_WINDOW:
				_focused_window_id = _focus_history.next(_focused_window_id, visible);
				_action.focus(_focused_window_id);
				return;

			case Command::PREV_WINDOW:
				_focused_window_id = _focus_history.prev(_focused_window_id, visible);
				_action.focus(_focused_window_id);
				return;

			case Command::SCREEN:
				_action.screen(command.target);
				return;

			case Command::RELEASE_GRAB:
				_action.release_grab();
				return;

			case Command::PICK_UP:
				if (_focused_window_id.value) {
					_picked_up = true;
					_action.pick_up(_focused_window_id);
				}
				return;

			case Command::PLACE_DOWN:
				if (_picked_up) {
					_action.place_down();
					_picked_up = false;
				}
				return;

			case Command::FREE_ARRANGE:
				_free_arrange = true;
				return;

			case Command::STRICT_ARRANGE:
				_free_arrange = false;
				return;

			case Command::DRAG:

				/* ignore clicks outside of a window in free-arrange mode */
				if (_free_arrange && !_hovered_window_id.valid())
					return;

				_drag_state      = true;
				_pointer_clicked = _pointer_curr;

				if (_hovered_window_id.valid()) {

					/*
					 * Initiate drag operation
					 *
					 * If the hovered window is known at the time of the press event,
					 * we can initiate the drag operation immediately. Otherwise,
					 * the initiation is deferred to the next update of the hover
					 * model.
					 */

					_initiate_drag(_hovered_window_id, _hovered_element());

				} else {

					/*
					 * If the hovering state is undefined at the time of the click,
					 * we defer the drag handling until the next update of the hover
					 * state. This intermediate state is captured by '_drag_init_done'.
					 */
					_drag_init_done    = false;
					_dragged_window_id = Window_id();
					_dragged_element   = Window::Element(Window::Element::UNDEFINED);
				}
				return;

			case Command::DROP:

				if (_drag_state && _dragged_window_id.valid()) {

					/*
					 * Issue resize to 0x0 when releasing the the window closer
					 */
					if (_dragged_element.closer())
						if (_dragged_element == _hovered_element())
							_action.close(_dragged_window_id);

					_action.finalize_drag(_dragged_window_id, _dragged_element,
					                      _pointer_clicked, _pointer_curr);
				}
				_drag_state = false;
				return;

			case Command::NONE:
				return;
			}
		});
	}

	if (_free_arrange && (!orig_free_arrange || orig_pointer_curr != _pointer_curr)) {
		Window::Element const orig_free_hovered_element = _free_hovered_element;
		_update_free_hovered_element();
		if (orig_free_hovered_element != _free_hovered_element)
			_action.free_arrange_hover_changed();
	}

	/* update focus history after key/button action is completed */
	if (e.release() && _key_cnt == 0)
		_focus_history.focus(_focused_window_id);
}

#endif /* _USER_STATE_H_ */
