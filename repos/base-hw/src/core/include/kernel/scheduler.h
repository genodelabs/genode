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
#include <kernel/configuration.h>
#include <kernel/double_list.h>
#include <assert.h>

namespace Kernel
{
	/**
	 * Range save priority value
	 */
	class Priority;

	/**
	 * Inheritable ability for objects of type T to be item in a scheduler
	 */
	template <typename T>
	class Scheduler_item;

	/**
	 * Round robin scheduler for objects of type T
	 */
	template <typename T>
	class Scheduler;
}

class Kernel::Priority
{
	private:

		unsigned _value;

	public:

		enum {
			MIN = 0,
			MAX = MAX_PRIORITY,
		};

		/**
		 * Constructor
		 */
		Priority(unsigned const priority)
		:
			_value(Genode::min(priority, MAX))
		{ }

		/**
		 * Assignment operator
		 */
		Priority & operator =(unsigned const priority)
		{
			_value = Genode::min(priority, MAX);
			return *this;
		}

		operator unsigned() const { return _value; }
};

/**
 * Ability to be item in a scheduler through inheritance
 */
template <typename T>
class Kernel::Scheduler_item : public Double_list<T>::Item
{
	private:

		Priority const _priority;

	protected:

		/**
		 * Return wether this item is managed by a scheduler currently
		 */
		bool _scheduled() const { return Double_list<T>::Item::_listed(); }

	public:

		/**
		 * Constructor
		 *
		 * \param p  scheduling priority
		 */
		Scheduler_item(Priority const p) : _priority(p) { }


		/***************
		 ** Accessors **
		 ***************/

		Priority priority() const { return _priority; }
};

template <typename T>
class Kernel::Scheduler
{
	private:

		T * const      _idle;
		T *            _occupant;
		Double_list<T> _items[Priority::MAX + 1];
		bool           _yield;

		bool _check_update(T * const occupant)
		{
			if (_yield) {
				_yield = false;
				return true;
			}
			if (_occupant != occupant) { return true; }
			return false;
		}

	public:

		typedef Scheduler_item<T> Item;

		/**
		 * Constructor
		 */
		Scheduler(T * const idle) : _idle(idle), _occupant(0) { }

		/**
		 * Adjust occupant reference to the current scheduling plan
		 *
		 * \return  updated occupant reference
		 */
		T * update_occupant(bool & update)
		{
			for (int i = Priority::MAX; i >= 0 ; i--) {
				T * const head = _items[i].head();
				if (!head) { continue; }
				update = _check_update(head);
				_occupant = head;
				return head;
			}
			update = _check_update(_idle);
			_occupant = 0;
			return _idle;
		}

		/**
		 * Adjust scheduling plan to the fact that the current occupant yileds
		 */
		void yield_occupation()
		{
			_yield = true;
			if (!_occupant) { return; }
			_items[_occupant->priority()].head_to_tail();
		}

		/**
		 * Include 'i' in scheduling
		 */
		void insert(T * const i)
		{
			assert(i != _idle);
			_items[i->priority()].insert_tail(i);
		}

		/**
		 * Include item in scheduling and check wether an update is needed
		 *
		 * \param item  targeted item
		 *
		 * \return  wether the current occupant is out-dated after insertion
		 */
		bool insert_and_check(T * const item)
		{
			insert(item);
			if (!_occupant) { return true; }
			return item->priority() > _occupant->priority();
		}

		/**
		 * Exclude 'i' from scheduling
		 */
		void remove(T * const i) { _items[i->priority()].remove(i); }


		/***************
		 ** Accessors **
		 ***************/

		T * occupant() { return _occupant ? _occupant : _idle; }

		T * idle() const { return _idle; }
};

#endif /* _KERNEL__SCHEDULER_H_ */
