/*
 * \brief  List which appends new elements at the end
 * \author Christian Prochaska
 * \date   2011-09-09
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _APPEND_LIST_H_
#define _APPEND_LIST_H_

/*
 * \param LT  list element type
 */
template <typename LT>
class Append_list
{
	private:

		LT *_first;
		LT *_last;

	public:

		class Element
		{
			protected:

				friend class Append_list;

				LT *_next;

			public:

				Element(): _next(0) { }

				/**
				 * Return next element in list
				 */
				LT *next() const { return _next; }
		};

	public:

		/**
		 * Constructor
		 *
		 * Start with an empty list.
		 */
		Append_list(): _first(0), _last(0) { }

		/**
		 * Return first list element
		 */
		LT *first() const { return _first; }

		/**
		 * Append element to list
		 */
		void append(LT *le)
		{
			if (_last) {
				_last->Element::_next = le;
				_last = le;
			} else {
				_first = _last = le;
			}
		}

		/**
		 * Remove element from list
		 */
		void remove(LT *le)
		{
			if (!_first) return;

			/* if specified element is the first of the list */
			if (le == _first) {
				if (_first == _last)
					_first = _last = 0;
				else
					_first = le->Element::_next;
			}

			else {

				/* search specified element in the list */
				LT *e = _first;
				while (e->_next && (e->_next != le))
					e = e->_next;

				/* element is not member of the list */
				if (!e->_next) return;

				/* e->_next is the element to remove, skip it in list */
				e->Element::_next = e->Element::_next->Element::_next;
				if (le == _last)
					_last = e;
			}

			le->Element::_next = 0;
		}
};

#endif /* _APPEND_LIST_H_ */
