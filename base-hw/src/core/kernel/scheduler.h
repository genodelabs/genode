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
#include <kernel/priority.h>
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

	/**
	 * Kernel object that can be scheduled for the CPU
	 */
	class Execution_context;

	typedef Scheduler<Execution_context> Cpu_scheduler;

	/**
	 * Return the systems CPU scheduler
	 */
	Cpu_scheduler * cpu_scheduler();
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


		/***************
		 ** Accessors **
		 ***************/

		Double_list<T> * list() { return _list; }
};

template <typename T>
class Kernel::Scheduler
{
	public:

		/**
		 * Capability to be item in a scheduler through inheritance
		 */
		struct Item : public Double_list<T>::Item
		{
			Priority priority;
		};

	protected:

		T * const      _idle;
		T *            _current;
		Double_list<T> _items[Priority::MAX+1];

	public:

		/**
		 * Constructor
		 */
		Scheduler(T * const idle) : _idle(idle), _current(0) { }

		/**
		 * Get currently scheduled item
		 */
		T * head()
		{
			for (int i = Priority::MAX; i >= 0 ; i--) {
				_current = _items[i].head();
				if (_current) return _current;
			}
			return _idle;
		}

		/**
		 * End turn of currently scheduled item
		 */
		void yield()
		{
			if (!_current) return;
			_items[_current->priority].head_to_tail();
		}

		/**
		 * Include 'i' in scheduling
		 */
		void insert(T * const i)
		{
			assert(i != _idle);
			_items[i->priority].insert_tail(i);
		}

		/**
		 * Exclude 'i' from scheduling
		 */
		void remove(T * const i) { _items[i->priority].remove(i); }
};

class Kernel::Execution_context : public Cpu_scheduler::Item
{
	public:

		/**
		 * Handle an exception that occured during execution
		 */
		virtual void handle_exception() = 0;

		/**
		 * Continue execution
		 */
		virtual void proceed() = 0;

		/**
		 * Destructor
		 */
		virtual ~Execution_context()
		{
			if (list()) { cpu_scheduler()->remove(this); }
		}
};

#endif /* _KERNEL__SCHEDULER_H_ */
