/*
 * \brief  Nitpicker mode
 * \author Norman Feske
 * \date   2006-08-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		Session *_next_focused_session = nullptr;

	protected:

		/*
		 * True while a global key sequence is processed
		 */
		bool _global_key_sequence = false;

	public:

		virtual ~Mode() { }

		/**
		 * Accessors
		 */
		bool drag() const { return _key_cnt > 0; }

		void inc_key_cnt() { _key_cnt++; }
		void dec_key_cnt() { _key_cnt--; }

		bool has_key_cnt(unsigned cnt) const { return cnt == _key_cnt; }

		bool key_pressed() const { return _key_cnt > 0; }

		Session       *focused_session()       { return _focused_session; }
		Session const *focused_session() const { return _focused_session; }

		virtual void focused_session(Session *session)
		{
			_focused_session      = session;
			_next_focused_session = session;
		}

		bool focused(Session const &session) const { return &session == _focused_session; }

		void next_focused_session(Session *session) { _next_focused_session = session; }

		/**
		 * Apply pending focus-change request that was issued during drag state
		 */
		void apply_pending_focus_change()
		{
			/*
			 * Defer focus changes to a point where no drag operation is in
			 * flight because otherwise, the involved sessions would obtain
			 * inconsistent press and release events. However, focus changes
			 * during global key sequences are fine.
			 */
			if (key_pressed() && !_global_key_sequence)
				return;

			if (_focused_session != _next_focused_session)
				_focused_session = _next_focused_session;
		}

		/**
		 * Discard all references to specified view
		 */
		virtual void forget(Session const &session)
		{
			if (&session == _focused_session)      _focused_session      = nullptr;
			if (&session == _next_focused_session) _next_focused_session = nullptr;
		}
};

#endif
