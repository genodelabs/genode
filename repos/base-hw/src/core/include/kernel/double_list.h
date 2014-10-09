/*
 * \brief   List of double connected items
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

namespace Kernel
{
	/**
	 * Ability to be an item in a double connected list
	 */
	class Double_list_item;

	/**
	 * List of double connected items
	 */
	class Double_list;

	/**
	 * Double list over objects of type 'T' that inherits from double-list item
	 */
	template <typename T> class Double_list_typed;
}

class Kernel::Double_list_item
{
	friend class Double_list;

	private:

		Double_list_item * _next;
		Double_list_item * _prev;
};

class Kernel::Double_list
{
	private:

		typedef Double_list_item Item;

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
		Double_list(): _head(0), _tail(0) { }

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
			for (Item * i = _head; i; i = i->_next) { f(i); } }

		/*
		 * Accessors
		 */

		Item *        head() const         { return _head; }
		static Item * next(Item * const i) { return i->_next; }
};

template <typename T> class Kernel::Double_list_typed : public Double_list
{
	private:

		typedef Double_list_item Item;

		static T * _typed(Item * const i) {
			return i ? static_cast<T *>(i) : 0; }

	public:

		/*
		 * 'Double_list' interface
		 */

		template <typename F> void for_each(F f) {
			Double_list::for_each([&] (Item * const i) { f(_typed(i)); }); }

		void to_tail(T * const t) { Double_list::to_tail(t); }
		void insert_tail(T * const t) { Double_list::insert_tail(t); }
		void insert_head(T * const t) { Double_list::insert_head(t); }
		void remove(T * const t) { Double_list::remove(t); }
		static T * next(T * const t) { return _typed(Double_list::next(t)); }
		T * head() const { return _typed(Double_list::head()); }
};

#endif /* _KERNEL__DOUBLE_LIST_H_ */
