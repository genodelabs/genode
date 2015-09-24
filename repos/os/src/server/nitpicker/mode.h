/*
 * \brief  Nitpicker mode
 * \author Norman Feske
 * \date   2006-08-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MODE_H_
#define _MODE_H_

class Session;

class Mode
{
	private:

		bool _xray = false;
		bool _kill = false;

		/*
		 * Number of currently pressed keys.
		 * This counter is used to determine if the user
		 * is dragging an item.
		 */
		unsigned _key_cnt = 0;

		Session *_focused_session = nullptr;

	public:

		virtual ~Mode() { }

		/**
		 * Accessors
		 */
		bool xray() const { return _xray; }
		bool kill() const { return _kill; }
		bool flat() const { return !_xray && !_kill; }
		bool drag() const { return _key_cnt > 0; }

		void leave_kill()  { _kill = false; }
		void toggle_kill() { _kill = !_kill; }
		void toggle_xray() { _xray = !_xray; }

		void inc_key_cnt() { _key_cnt++; }
		void dec_key_cnt() { _key_cnt--; }

		bool has_key_cnt(unsigned cnt) const { return cnt == _key_cnt; }

		bool key_is_pressed() const { return _key_cnt > 0; }

		Session       *focused_session()       { return _focused_session; }
		Session const *focused_session() const { return _focused_session; }

		virtual void focused_session(Session *session) { _focused_session = session; }

		bool is_focused(Session const &session) const { return &session == _focused_session; }

		/**
		 * Discard all references to specified view
		 */
		virtual void forget(Session const &session)
		{
			if (is_focused(session)) _focused_session = nullptr;
		}
};

#endif
