/*
 * \brief   Round-robin scheduler
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__SCHEDULER_H_
#define _KERNEL__SCHEDULER_H_

/* core includes */
#include <assert.h>

namespace Kernel
{
	/**
	 * Double connected list of objects of type T
	 */
	template <typename T>
	class Double_list;

	/**
	 * Round robin scheduler for objects of type T
	 */
	template <typename T>
	class Scheduler;
}

template <typename T>
class Kernel::Double_list
{
	public:

		/**
		 * Enable deriving objects to be inserted into a double list
		 */
		class Item;

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
		void insert_tail(T * const t)
		{
			Item * i = static_cast<Item *>(t);
			assert(i && !i->Item::_list);

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
		void remove(T * const t)
		{
			Item * i = static_cast<Item *>(t);
			assert(_head && i && i->Item::_list == this);

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


		/***************
		 ** Accessors **
		 ***************/

		T * head() const { return static_cast<T *>(_head); }
};

template <typename T>
class Kernel::Double_list<T>::Item
{
	friend class Double_list<T>;

	private:

		Item *           _next;
		Item *           _prev;
		Double_list<T> * _list;

	public:

		/**
		 * Constructor
		 */
		Item() : _next(0), _prev(0), _list(0) { }
};

template <typename T>
class Kernel::Scheduler
{
	public:

		/**
		 * Capability to be item in a scheduler through inheritance
		 */
		class Item : public Double_list<T>::Item { };

	protected:

		T * const      _idle;
		Double_list<T> _items;

	public:

		/**
		 * Constructor
		 */
		Scheduler(T * const idle) : _idle(idle) { }

		/**
		 * Get currently scheduled item
		 */
		T * head() const
		{
			T * const i = _items.head();
			if (i) { return i; }
			return _idle;
		}

		/**
		 * End turn of currently scheduled item
		 */
		void yield() { _items.head_to_tail(); }

		/**
		 * Include 'i' in scheduling
		 */
		void insert(T * const i)
		{
			assert(i != _idle);
			_items.insert_tail(i);
		}

		/**
		 * Exclude 'i' from scheduling
		 */
		void remove(T * const i) { _items.remove(i); }
};

#endif /* _KERNEL__SCHEDULER_H_ */
