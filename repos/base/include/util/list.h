/*
 * \brief  Single connected list
 * \author Norman Feske
 * \date   2006-08-02
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__LIST_H_
#define _INCLUDE__UTIL__LIST_H_

namespace Genode {

	template <typename> class List;
	template <typename> class List_element;
}


/**
 * Single-connected list
 *
 * \param LT  list element type
 */
template <typename LT>
class Genode::List
{
	private:

		LT *_first;

	public:

		class Element
		{
			protected:

				friend class List;

				LT mutable *_next;

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
		 */
		List() : _first(0) { }

		/**
		 * Return first list element
		 */
		LT       *first()       { return _first; }
		LT const *first() const { return _first; }

		/**
		 * Insert element after specified element into list
		 *
		 * \param  le  list element to insert
		 * \param  at  target position (preceding list element)
		 */
		void insert(LT const *le, LT const *at = 0)
		{
			/* insert at beginning of the list */
			if (at == 0) {
				le->List::Element::_next = _first;
				_first = const_cast<LT *>(le);
			} else {
				le->List::Element::_next = at->List::Element::_next;
				at->List::Element::_next = const_cast<LT *>(le);
			}
		}

		/**
		 * Remove element from list
		 */
		void remove(LT const *le)
		{
			if (!_first) return;

			/* if specified element is the first of the list */
			if (le == _first) {
				_first = le->List::Element::_next;

			} else {

				/* search specified element in the list */
				Element *e = _first;
				while (e->_next && (e->_next != le))
					e = e->_next;

				/* element is not member of the list */
				if (!e->_next) return;

				/* e->_next is the element to remove, skip it in list */
				e->List::Element::_next = e->List::Element::_next->List::Element::_next;
			}

			le->List::Element::_next = 0;
		}
};


/**
 * Helper for using member variables as list elements
 *
 * \param T  type of compound object to be organized in a list
 *
 * This helper allow the creation of lists that use member variables to
 * connect their elements. This way, the organized type does not need to
 * publicly inherit 'List<LT>::Element'. Furthermore objects can easily
 * be organized in multiple lists by embedding multiple 'List_element'
 * member variables.
 */
template <typename T>
class Genode::List_element : public List<List_element<T> >::Element
{
	T *_object;

	public:

		List_element(T *object) : _object(object) { }

		T *object() const { return _object; }
};

#endif /* _INCLUDE__UTIL__LIST_H_ */
