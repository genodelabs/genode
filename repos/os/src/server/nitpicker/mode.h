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

		/*
		 * Number of currently pressed keys. This counter is used to determine
		 * if the user is dragging an item.
		 */
		unsigned _key_cnt = 0;

		Session *_focused_session = nullptr;

	public:

		virtual ~Mode() { }

		/**
		 * Accessors
		 */
		bool drag() const { return _key_cnt > 0; }

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
