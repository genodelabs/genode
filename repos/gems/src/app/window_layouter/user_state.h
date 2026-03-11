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
#include <queue.h>

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
		 * Stores to-be-processed commands for when the hover report has been
		 * updated for the corresponding sequence number. 
		 */
		using Command_queue = Queue<Deferred_command, 32>;
		Command_queue _deferred_commands { };

		/* last seen sequence number */
		Input::Seq_number _last_seq_number { };

		/* sequence number of last hover report */
		Input::Seq_number _hovered_seq_number { };

		/*
		 * True while drag operation in progress
		 */
		bool _drag_state = false;

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

		inline bool _handle_command(Command const &, Input::Seq_number, Point);

		inline void _handle_event(Input::Event const &, Node const &);

		inline void _defer_command(Command const &command)
		{
			if (_deferred_commands.avail_capacity())
				_deferred_commands.enqueue({ command, _last_seq_number, _pointer_curr });
			else
				error("Unable to defer command: command queue is full");
		}

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

		void _update_free_hovered_element(Input::Seq_number const seq_number,
		                                  Point             const pointer)
		{
			if (seq_number.value != _hovered_seq_number.value)
				return;

			Window::Element const orig_free_hovered_element = _free_hovered_element;

			_free_hovered_element = { };
			if (_hovered_window_id.valid())
				_free_hovered_element = _action.free_arrange_element_at(_hovered_window_id,
				                                                        pointer);

			if (_free_arrange && orig_free_hovered_element != _free_hovered_element)
				_action.free_arrange_hover_changed();
		}

	public:

		User_state(Action &action, Focus_history &focus_history)
		:
			_action(action), _focus_history(focus_history)
		{ }

		void handle_input(Input::Event const events[], unsigned num_events,
		                  Node const &config)
		{
			for (size_t i = 0; i < num_events; i++)
				_handle_event(events[i], config);
		}

		void hover(Window_id window_id, Window::Element element,
		           Input::Seq_number seq_number)
		{
			Window_id const orig_hovered_window_id = _hovered_window_id;

			_hovered_window_id      = window_id;
			_strict_hovered_element = element;
			_hovered_seq_number     = seq_number;

			bool const had_deferred_commands = !_deferred_commands.empty();

			/*
			 * process deferred commands until queue is empty or command
			 * could not be handled, yet
			 */
			bool handled = true;
			while (handled)
			{
				handled = _deferred_commands.dequeue_if([&] (Deferred_command const &c) {
					return _handle_command(c.command, c.seq_number, c.pointer);
				});
			}

			/* only update free-hover state and and focus if all deferred commands
			 * have been processed */
			if (!_deferred_commands.empty())
				return;

			/* update drag with current pointer position */
			if (_drag_state && had_deferred_commands)
				_action.drag(_dragged_window_id, _dragged_element,
				             _pointer_clicked, _pointer_curr);

			/* update free hovered element with current pointer and hover */
			_update_free_hovered_element(_last_seq_number, _pointer_curr);

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

		void reset_hover(Input::Seq_number seq_number)
		{
			/* ignore hover resets when in drag state */
			if (_drag_state)
				return;

			_strict_hovered_element = { };
			_hovered_window_id      = { };
			_hovered_seq_number     = seq_number;
		}

		Window_id focused_window_id() const { return _focused_window_id; }

		void focused_window_id(Window_id id) { _focused_window_id = id; }

		Hover_state hover_state() const
		{
			return { .window_id = _hovered_window_id,
			         .element   = _hovered_element() };
		}
};


bool Window_layouter::User_state::_handle_command(Command           const &command,
                                                  Input::Seq_number const  seq_number,
                                                  Point             const  pointer)
{
	auto visible = [&] (Window_id id) { return _action.visible(id); };

	switch (command.type) {

	case Command::TOGGLE_FULLSCREEN:
		_action.toggle_fullscreen(_focused_window_id);
		break;

	case Command::RAISE_WINDOW:
		_action.to_front(_focused_window_id);
		break;

	case Command::NEXT_WINDOW:
		_focused_window_id = _focus_history.next(_focused_window_id, visible);
		_action.focus(_focused_window_id);
		break;

	case Command::PREV_WINDOW:
		_focused_window_id = _focus_history.prev(_focused_window_id, visible);
		_action.focus(_focused_window_id);
		break;

	case Command::SCREEN:
		_action.screen(command.target);
		break;

	case Command::RELEASE_GRAB:
		_action.release_grab();
		break;

	case Command::PICK_UP:
		if (_focused_window_id.value) {
			_picked_up = true;
			_action.pick_up(_focused_window_id);
		}
		break;

	case Command::PLACE_DOWN:
		if (_picked_up) {
			_action.place_down();
			_picked_up = false;
		}
		break;

	case Command::FREE_ARRANGE:
		if (!_free_arrange)
			_update_free_hovered_element(seq_number, pointer);

		_free_arrange = true;
		break;

	case Command::STRICT_ARRANGE:
		_free_arrange = false;
		break;

	case Command::DRAG:

		// defer if hover report is not up-to-date
		if (seq_number.value > _hovered_seq_number.value)
			return false;

		/* ignore command if hover report is ahead */
		if (seq_number.value < _hovered_seq_number.value)
			return true;

		/* ignore clicks outside of a window in free-arrange mode */
		if (_free_arrange && !_hovered_window_id.valid())
			return true;

		/* update free arrange hover state with clicked position */
		if (_free_arrange)
			_update_free_hovered_element(seq_number, pointer);

		if (_hovered_window_id.valid()) {

			_drag_state      = true;
			_pointer_clicked = pointer;

			/*
			 * Initiate drag operation
			 *
			 * If the hovered window is known at the time of the press event,
			 * we can initiate the drag operation immediately. Otherwise,
			 * the initiation is deferred to the next update of the hover
			 * model.
			 */

			_initiate_drag(_hovered_window_id, _hovered_element());

		} else
			error("cannot drag: undefined hover state");
		break;

	case Command::DROP:

		if (_drag_state && _dragged_window_id.valid()) {

			/*
			 * Issue resize to 0x0 when releasing the the window closer
			 */
			if (_dragged_element.closer())
				if (_dragged_element == _hovered_element())
					_action.close(_dragged_window_id);

			_action.finalize_drag(_dragged_window_id, _dragged_element,
			                      _pointer_clicked, pointer);
		}
		_drag_state = false;
		break;

	case Command::IDLE:
		_focus_history.focus(_focused_window_id);
		break;

	case Command::NONE:
		break;
	}
	
	/* command handled */
	return true;
}


void Window_layouter::User_state::_handle_event(Input::Event const &e,
                                                Node const &config)
{
	Point const orig_pointer_curr = _pointer_curr;

	e.handle_seq_number([&] (Input::Seq_number seq_number) {
		_last_seq_number = seq_number; });

	e.handle_absolute_motion([&] (int x, int y) {
		_pointer_curr = Point(x, y); });

	e.handle_touch([&] (Input::Touch_id id, float x, float y) {
		if (id.value == 0)
			_pointer_curr = Point((int)x, (int)y); });

	/* track number of pressed buttons/keys */
	if (e.press())   _key_cnt++;
	if (e.release()) _key_cnt--;

	/* handle key sequences */
	if (_key(e)) {

		if (e.press() && _key_cnt == 1)
			_key_sequence_tracker.reset();

		_key_sequence_tracker.apply(e, config, [&] (Command const &command) {

			/* defer command if queue is not empty */
			if (!_deferred_commands.empty()) {
				_defer_command(command);
				return;
			}

			/* handle command immediately or enqueue */
			if (!_handle_command(command, _last_seq_number, _pointer_curr))
				_defer_command(command);
		});
	}

	/* update focus history after key/button action is completed */
	if (e.release() && _key_cnt == 0) {
		if (_deferred_commands.empty())
			_focus_history.focus(_focused_window_id);
		else
			_defer_command({ .type = Command::Type::IDLE, .target = Target::Name() });
	}

	/*
	 * only update drag state and free-hovered state when all deferred
	 * commands have been processed
	 */
	if (!_deferred_commands.empty())
		return;

	if (e.focus_enter() || orig_pointer_curr != _pointer_curr) {
		if (_drag_state)
			_action.drag(_dragged_window_id, _dragged_element,
			             _pointer_clicked, _pointer_curr);
	}

	if (_free_arrange && orig_pointer_curr != _pointer_curr)
		_update_free_hovered_element(_last_seq_number, _pointer_curr);
}

#endif /* _USER_STATE_H_ */
