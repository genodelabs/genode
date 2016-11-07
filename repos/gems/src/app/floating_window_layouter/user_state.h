/*
 * \brief  Floating window layouter
 * \author Norman Feske
 * \date   2013-02-14
 */

/*
 * Copyright (C) 2013-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _USER_STATE_H_
#define _USER_STATE_H_

/* local includes */
#include "operations.h"
#include "key_sequence_tracker.h"

namespace Floating_window_layouter { class User_state; }


class Floating_window_layouter::User_state
{
	public:

		struct Hover_state
		{
			Window_id       window_id;
			Window::Element element { Window::Element::UNDEFINED };

			Hover_state(Window_id id, Window::Element element)
			:
				window_id(id), element(element)
			{ }
		};

	private:

		Window_id _hovered_window_id;
		Window_id _focused_window_id;
		Window_id _dragged_window_id;

		unsigned  _key_cnt = 0;

		Key_sequence_tracker _key_sequence_tracker;

		Window::Element _hovered_element = Window::Element::UNDEFINED;
		Window::Element _dragged_element = Window::Element::UNDEFINED;

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

		/*
		 * Pointer position at the beginning of a drag operation
		 */
		Point _pointer_clicked;

		/*
		 * Current pointer position
		 */
		Point _pointer_curr;

		Operations &_operations;

		Focus_history &_focus_history;

		bool _key(Input::Event const &ev) const
		{
			if (ev.type() != Input::Event::PRESS
			 && ev.type() != Input::Event::RELEASE)
				return false;

			return ev.keycode() != Input::BTN_LEFT;
		}

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
			if (_hovered_element == Window::Element::MAXIMIZER) {

				_dragged_window_id = _hovered_window_id;
				_focused_window_id = _hovered_window_id;
				_focus_history.focus(_focused_window_id);

				_operations.toggle_fullscreen(_hovered_window_id);
				return;
			}

			/*
			 * Bring hovered window to front when clicked
			 */
			if (_focused_window_id != _hovered_window_id) {

				_focused_window_id = _hovered_window_id;
				_focus_history.focus(_focused_window_id);

				_operations.to_front(_hovered_window_id);
				_operations.focus(_hovered_window_id);
			}

			_operations.drag(_dragged_window_id, _dragged_element,
			                 _pointer_clicked, _pointer_curr);
		}

	public:

		User_state(Operations &operations, Focus_history &focus_history)
		:
			_operations(operations), _focus_history(focus_history)
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
				_operations.drag(_dragged_window_id, _dragged_element,
				                 _pointer_clicked, _pointer_curr);
		}

		void hover(Window_id window_id, Window::Element element)
		{
			Window_id const last_hovered_window_id = _hovered_window_id;

			_hovered_window_id = window_id;
			_hovered_element   = element;

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
				_initiate_drag(_hovered_window_id, _hovered_element);

			/*
			 * Let focus follows the pointer
			 *
			 * XXX obtain policy from config
			 */
			if (!_drag_state && _hovered_window_id.valid()
			 && _hovered_window_id != last_hovered_window_id) {

				_focused_window_id = _hovered_window_id;
				_focus_history.focus(_focused_window_id);
				_operations.focus(_focused_window_id);
			}
		}

		void reset_hover()
		{
			/* ignore hover resets when in drag state */
			if (_drag_state)
				return;

			_hovered_element   = Window::Element::UNDEFINED;
			_hovered_window_id = Window_id();
		}

		Window_id focused_window_id() const { return _focused_window_id; }

		void focused_window_id(Window_id id) { _focused_window_id = id; }

		Hover_state hover_state() const { return { _hovered_window_id, _hovered_element }; }
};


void Floating_window_layouter::User_state::_handle_event(Input::Event const &e,
                                                         Xml_node config)
{
	if (e.type() == Input::Event::MOTION
	 || e.type() == Input::Event::FOCUS) {

		_pointer_curr = Point(e.ax(), e.ay());

		if (_drag_state && _drag_init_done)
			_operations.drag(_dragged_window_id, _dragged_element,
			                 _pointer_clicked, _pointer_curr);
	}

	/* track number of pressed buttons/keys */
	if (e.type() == Input::Event::PRESS)   _key_cnt++;
	if (e.type() == Input::Event::RELEASE) _key_cnt--;

	/* handle pointer click */
	if (e.type()    == Input::Event::PRESS
	 && e.keycode() == Input::BTN_LEFT
	 && _key_cnt    == 1) {

		/*
		 * Initiate drag operation if possible
		 */
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

			_initiate_drag(_hovered_window_id, _hovered_element);

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
	}

	/* detect end of drag operation */
	if (e.type() == Input::Event::RELEASE
	 && _key_cnt == 0
	 && _dragged_window_id.valid()) {

		_drag_state = false;

		/*
		 * Issue resize to 0x0 when releasing the the window closer
		 */
		if (_dragged_element == Window::Element::CLOSER) {

			if (_dragged_element == _hovered_element)
				_operations.close(_dragged_window_id);
		}

		_operations.finalize_drag(_dragged_window_id, _dragged_element,
		                          _pointer_clicked, _pointer_curr);
	}

	/* handle key sequences */
	if (_key(e)) {

		if (e.type() == Input::Event::PRESS && _key_cnt == 1)
			_key_sequence_tracker.reset();

		_key_sequence_tracker.apply(e, config, [&] (Action action) {

			switch (action.type()) {

			case Action::TOGGLE_FULLSCREEN:
				_operations.toggle_fullscreen(_focused_window_id);
				return;

			case Action::RAISE_WINDOW:
				_operations.to_front(_focused_window_id);
				return;

			case Action::NEXT_WINDOW:
				_focused_window_id = _focus_history.next(_focused_window_id);
				_operations.focus(_focused_window_id);
				return;

			case Action::PREV_WINDOW:
				_focused_window_id = _focus_history.prev(_focused_window_id);
				_operations.focus(_focused_window_id);
				return;

			default:
				Genode::warning("action ", (int)action.type(), " unhanded");
			}
		});
	}

	/* update focus history after key/button action is completed */
	if (e.type() == Input::Event::RELEASE && _key_cnt == 0)
		_focus_history.focus(_focused_window_id);
}

#endif /* _USER_STATE_H_ */
