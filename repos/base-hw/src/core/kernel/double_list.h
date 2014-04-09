/*
 * \brief   Double connected list
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__DOUBLE_LIST_H_
#define _KERNEL__DOUBLE_LIST_H_

/* core includes */
#include <assert.h>

namespace Kernel
{
	/**
	 * Ability to be an item in a double connected list
	 *
	 * \param T  object type that inherits from Double_list_item<T>
	 */
	template <typename T>
	class Double_list_item;

	/**
	 * Double connected list
	 *
	 * \param T  object type that inherits from Double_list_item<T>
	 */
	template <typename T>
	class Double_list;
}

template <typename T>
class Kernel::Double_list_item
{
	friend class Double_list<T>;

	private:

		Double_list_item<T> * _next;
		Double_list_item<T> * _prev;
		Double_list<T> *      _list;

		/**
		 * Return the object behind this item
		 */
		T * _object() { return static_cast<T *>(this); }

	protected:

		/**
		 * Return wether this item is managed by a list currently
		 */
		bool _listed() const { return _list; }

	public:

		/**
		 * Constructor
		 */
		Double_list_item() : _next(0), _prev(0), _list(0) { }
};

template <typename T>
class Kernel::Double_list
{
	public:

		typedef Double_list_item<T> Item;

	private:

		Item * _head;
		Item * _tail;

	public:

		/**
		 * Constructor
		 */
		Double_list(): _head(0), _tail(0) { }

		/**
		 * Insert item 't' from behind into list
		 */
		void insert_tail(Item * const i)
		{
			/* assertions */
			assert(!i->_list);

			/* update new item */
			i->_prev = _tail;
			i->_next = 0;
			i->_list = this;

			/* update rest of the list */
			if (_tail) { _tail->_next = i; }
			else { _head = i; }
			_tail = i;
		}

		/**
		 * Remove item 't' from list
		 */
		void remove(Item * const i)
		{
			/* assertions */
			assert(i->_list == this);

			/* update next item or _tail */
			if (i != _tail) { i->_next->_prev = i->_prev; }
			else { _tail = i->_prev; }

			/* update previous item or _head */
			if (i != _head) { i->_prev->_next = i->_next; }
			else { _head = i->_next; }

			/* update removed item */
			i->_list = 0;
		}

		/**
		 * Remove head from list and insert it at the end
		 */
		void head_to_tail()
		{
			/* exit if nothing to do */
			if (!_head || _head == _tail) { return; }

			/* remove head */
			Item * const i = _head;
			_head = _head->_next;
			i->_next = 0;
			_head->_prev = 0;

			/* insert tail */
			_tail->_next = i;
			i->_prev = _tail;
			_tail = i;
		}

		/**
		 * Call a function for each object in the list
		 *
		 * \param function  targeted function of type 'void function(T *)'
		 */
		template <typename Function>
		void for_each(Function function)
		{
			Item * i = _head;
			while (i) {
				function(i->_object());
				i = i->_next;
			}
		}

		/***************
		 ** Accessors **
		 ***************/

		T * head() const { return _head ? _head->_object() : 0; }
};

#endif /* _KERNEL__DOUBLE_LIST_H_ */
