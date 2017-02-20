/*
 * \brief   User state manager
 * \date    2005-11-16
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__USER_STATE_H_
#define _INCLUDE__SCOUT__USER_STATE_H_

#include <scout/window.h>

namespace Scout { class User_state; }


class Scout::User_state : public Parent_element
{
	private:

		Element *_mfocus;    /* element that owns the current mouse focus */
		Element *_active;    /* currently activated element               */
		Window  *_window;
		Element *_root;      /* root of element tree                      */
		int      _key_cnt;   /* number of currently pressed keys          */

		Point    _mouse_position;
		Point    _view_position;

		/**
		 * Assign new mouse focus element
		 */
		void _assign_mfocus(Element *e, int force = 0)
		{
			/* return if mouse focus did not change */
			if (!force && e == _mfocus) return;

			/* tell old mouse focus to release focus */
			if (_mfocus) _mfocus->mfocus(0);

			/* assign new current mouse focus */
			_mfocus = e;

			/* notify new mouse focus */
			if (_mfocus) _mfocus->mfocus(1);
		}

	public:

		/**
		 * Constructor
		 */
		User_state(Window *window, Element *root, int vx, int vy)
		:
			_mfocus(0), _active(0), _window(window), _root(root),
			_key_cnt(0), _view_position(Point(vx, vy))
		{ }

		/**
		 * Accessor functions
		 */
		Point mouse_position() const { return _mouse_position; }
		Point view_position()  const { return _view_position; }

		/**
		 * Update the current view offset
		 */
		void update_view_offset()
		{
			_view_position = Point(_window->view_x(), _window->view_y());
		}

		/**
		 * Apply input event to mouse focus state
		 */
		void handle_event(Event const &ev)
		{
			_key_cnt += ev.type == Event::PRESS   ? 1 : 0;
			_key_cnt -= ev.type == Event::RELEASE ? 1 : 0;

			if (_key_cnt < 0) _key_cnt = 0;

			if (_active)
				_active->handle_event(ev);

			/* find element under the mouse cursor */
			_mouse_position = ev.mouse_position;

			Element *e = _root->find(_mouse_position);

			switch (ev.type) {

				case Event::PRESS:

					if (_key_cnt != 1) break;
					if (!e) break;

					_active = e;
					_active->handle_event(ev);

					update_view_offset();

					_assign_mfocus(_root->find(ev.mouse_position), 1);

					break;

				case Event::RELEASE:

					if (_key_cnt == 0) {
						update_view_offset();
						_active = 0;
						_assign_mfocus(e);
					}
					break;

				case Event::MOTION:

					if (!_active && e) e->handle_event(ev);
					if (_key_cnt == 0)
						_assign_mfocus(e);
					break;

				case Event::WHEEL:

					if (_key_cnt == 0)
						_window->ypos(_window->ypos() + 23 * ev.mouse_position.y());
					break;

				default:

					break;
			}
		}


		/********************
		 ** Parent element **
		 ********************/

		void forget(Element const *e)
		{
			if (_mfocus == e) _mfocus = 0;
			if (_active == e) _active = 0;
		}
};

#endif /* _INCLUDE__SCOUT__USER_STATE_H_ */
