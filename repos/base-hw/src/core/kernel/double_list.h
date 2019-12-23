/*
 * \brief   List of double connected items
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__DOUBLE_LIST_H_
#define _CORE__KERNEL__DOUBLE_LIST_H_

namespace Kernel
{
	/**
	 * Ability to be an item in a double connected list
	 */
	template <typename T>
	class Double_list_item;

	/**
	 * List of double connected items
	 */
	template <typename T>
	class Double_list;
}

template <typename T>
class Kernel::Double_list_item
{
	friend class Double_list<T>;

	private:

		Double_list_item * _next = nullptr;
		Double_list_item * _prev = nullptr;
		T                & _payload;

	public:

		Double_list_item(T &payload) : _payload(payload) { }

		T &payload() { return _payload; }
};

template <typename T>
class Kernel::Double_list
{
	private:

		typedef Double_list_item<T> Item;

		Item * _head;
		Item * _tail;

		void _connect_neighbors(Item * const i)
		{
			i->_prev->_next = i->_next;
			i->_next->_prev = i->_prev;
		}

		void _to_tail(Item * const i)
		{
			if (i == _tail) { return; }
			_connect_neighbors(i);
			i->_prev = _tail;
			i->_next = 0;
			_tail->_next = i;
			_tail = i;
		}

	public:

		/**
		 * Construct empty list
		 */
		Double_list() : _head(0), _tail(0) { }

		/**
		 * Move item 'i' from its current list position to the tail
		 */
		void to_tail(Item * const i)
		{
			if (i == _head) { head_to_tail(); }
			else { _to_tail(i); }
		}

		/**
		 * Insert item 'i' as new tail into list
		 */
		void insert_tail(Item * const i)
		{
			if (_tail) { _tail->_next = i; }
			else { _head = i; }
			i->_prev = _tail;
			i->_next = 0;
			_tail = i;
		}

		/**
		 * Insert item 'i' as new head into list
		 */
		void insert_head(Item * const i)
		{
			if (_head) { _head->_prev = i; }
			else { _tail = i; }
			i->_next = _head;
			i->_prev = 0;
			_head = i;
		}

		/**
		 * Remove item 'i' from list
		 */
		void remove(Item * const i)
		{
			if (i == _tail) { _tail = i->_prev; }
			else { i->_next->_prev = i->_prev; }
			if (i == _head) { _head = i->_next; }
			else { i->_prev->_next = i->_next; }
		}

		/**
		 * Move head item of list to tail position
		 */
		void head_to_tail()
		{
			if (!_head || _head == _tail) { return; }
			_head->_prev = _tail;
			_tail->_next = _head;
			_head = _head->_next;
			_head->_prev = 0;
			_tail = _tail->_next;
			_tail->_next = 0;
		}

		/**
		 * Call function 'f' of type 'void (Item *)' for each item in the list
		 */
		template <typename F> void for_each(F f) {
			for (Item * i = _head; i; i = i->_next) { f(i->payload()); } }

		/*
		 * Accessors
		 */

		Item *        head() const         { return _head; }
		static Item * next(Item * const i) { return i->_next; }
};

#endif /* _CORE__KERNEL__DOUBLE_LIST_H_ */
