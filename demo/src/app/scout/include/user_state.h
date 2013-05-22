/*
 * \brief   User state manager
 * \date    2005-11-16
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _USER_STATE_H_
#define _USER_STATE_H_

#include "window.h"
#include "elements.h"


class User_state : public Parent_element
{
	private:

		Window  *_window;
		Element *_root;      /* root of element tree                      */
		Element *_mfocus;    /* element that owns the current mouse focus */
		Element *_dst;       /* current link destination                  */
		Element *_active;    /* currently activated element               */
		int      _key_cnt;   /* number of currently pressed keys          */
		int      _mx, _my;   /* current mouse position                    */
		int      _vx, _vy;   /* current view offset                       */

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

			/* determine new current link destination */
			Element *old_dst = _dst;
			if (_mfocus && _mfocus->is_link()) {
				Link_token *l = static_cast<Link_token *>(_mfocus);
				_dst = l->dst();
			} else
				_dst = 0;

			/* nofify element tree about new link destination */
			if (_dst != old_dst)
				_root->curr_link_destination(_dst);
		}

	public:

		/**
		 * Constructor
		 */
		User_state(Window *window, Element *root, int vx, int vy)
		{
			_mfocus  = _dst = _active = 0;
			_window  = window;
			_root    = root;
			_key_cnt = 0;
			_vx      = vx;
			_vy      = vy;
		}

		/**
		 * Accessor functions
		 */
		int mx() { return _mx; }
		int my() { return _my; }
		int vx() { return _vx; }
		int vy() { return _vy; }

		/**
		 * Update the current view offset
		 */
		void update_view_offset()
		{
			_vx = _window->view_x();
			_vy = _window->view_y();
		}

		/**
		 * Apply input event to mouse focus state
		 */
		void handle_event(Event &ev)
		{
			_key_cnt += ev.type == Event::PRESS   ? 1 : 0;
			_key_cnt -= ev.type == Event::RELEASE ? 1 : 0;

			if (_key_cnt < 0) _key_cnt = 0;

			if (_active)
				_active->handle_event(ev);

			/* find element under the mouse cursor */
			_mx = ev.mx;
			_my = ev.my;
			Element *e = _root->find(_mx, _my);

			switch (ev.type) {

				case Event::PRESS:

					if (_key_cnt != 1) break;
					if (!e) break;

					_active = e;
					_active->handle_event(ev);

					update_view_offset();

					_assign_mfocus(_root->find(ev.mx, ev.my), 1);

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
						_window->ypos(_window->ypos() + 23 * ev.my);
					break;

				default:

					break;
			}
		}


		/********************
		 ** Parent element **
		 ********************/

		void forget(Element *e)
		{
			if (_mfocus == e) _mfocus = 0;
			if (_dst    == e) _dst    = 0;
			if (_active == e) _active = 0;
		}
};

#endif /* _USER_STATE_H_ */
