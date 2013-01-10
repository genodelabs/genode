/*
 * \brief  Simple connected list
 * \author Norman Feske
 * \date   2006-08-02
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIST_H_
#define _LIST_H_

/*
 * \param LT  list element type
 */
template <typename LT>
class List
{
	private:

		LT *_first;

	public:

		class Element
		{
			protected:

				friend class List;

				LT *_next;

			public:

				Element(): _next(0) { }

				/**
				 * Return next element in list
				 */
				LT *next() { return _next; }
		};

	public:

		/**
		 * Constructor
		 *
		 * On construction, start with an empty list.
		 */
		List(): _first(0) { }

		/**
		 * Return first list element
		 */
		LT *first() { return _first; }

		/**
		 * Insert element into list
		 *
		 * \param  le  list element to insert
		 * \param  at  target position (preceding list element) or
		 *             0 to insert element at the beginning of the list
		 */
		void insert(LT *le, LT *at = 0)
		{
			/* insert element at the beginning of the list */
			if (at == 0) {
				le->_next = _first;
				_first    = le;
			} else {
				le->_next = at->_next;
				at->_next = le;
			}
		}

		/**
		 * Remove element from list
		 */
		void remove(LT *le)
		{
			if (!_first) return;

			/* if specified element is the first of the list */
			if (le == _first)
				_first = le->_next;

			else {

				/* search specified element in the list */
				LT *e = _first;
				while (e->_next && (e->_next != le))
					e = e->_next;

				/* element is not member of the list */
				if (!e->_next) return;

				/* e->_next is the element to remove, skip it in list */
				e->_next = e->_next->_next;
			}

			le->_next = 0;
		}
};

#endif
