/*
 * \brief   Browser history buffer
 * \date    2005-11-03
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _HISTORY_H_
#define _HISTORY_H_

#include "elements.h"

namespace Scout { class History; }


class Scout::History
{
	private:

		static const int _size = 128;    /* history size */

		int     _idx = 0;              /* current position in history       */
		Anchor *_history[_size];       /* ring buffer of history references */

		/**
		 * Increment history position
		 *
		 * \offset  The incrementation offset must be higher than -_size
		 */
		inline void _inc(int offset) { _idx = (_idx + _size + offset) % _size; }

		/**
		 * Return next index of current position
		 */
		inline int _next() { return (_idx + 1) % _size; }

		/**
		 * Return previous index of current position
		 */
		inline int _prev() { return (_idx + _size - 1) % _size; }

	public:

		/**
		 * Constructor
		 */
		History()
		{
			memset(_history, 0, sizeof(_history));
		}

		/**
		 * Request element at current history position
		 */
		Anchor *curr() { return _history[_idx]; }

		/**
		 * Add element to history.
		 *
		 * We increment the current history position and insert the
		 * new element there. If the new element is identical with
		 * with the old element at that position, we keep the following
		 * history elements. Otherwise, we follow a new link 'branch'
		 * and cut off the old one.
		 */
		void add(Anchor *e)
		{
			/* discard invalid history elements */
			if (!e) return;

			/* increment history position */
			_inc(1);

			/* do we just follow the forward path? */
			if (curr() == e) return;

			/* cut off old forward history branch */
			_history[_next()] = 0;

			/* insert new element */
			_history[_idx] = e;
		}

		/**
		 * Assign new element to current history entry
		 */
		void assign(Anchor *e) { if (e) _history[_idx] = e; }

		/**
		 * Travel forward or backward in history
		 *
		 * \return  1 on success,
		 *          0 if end of history is reached
		 */
		enum direction { FORWARD, BACKWARD };
		int go(direction dir)
		{
			/* stop at the boundaries of the history */
			if (!_history[dir == FORWARD ? _next() : _prev()]) return 0;

			/* travel in history */
			_inc(dir == FORWARD ? 1 : -1);
			return 1;
		}
};

#endif
