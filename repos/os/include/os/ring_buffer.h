/*
 * \brief  Ring buffer
 * \author Norman Feske
 * \date   2007-09-28
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__RING_BUFFER_H_
#define _INCLUDE__OS__RING_BUFFER_H_

#include <base/semaphore.h>
#include <base/exception.h>
#include <util/string.h>

namespace Genode {

	struct Ring_buffer_unsynchronized;
	struct Ring_buffer_synchronized;

	template <typename, int, typename SYNC_POLICY = Ring_buffer_synchronized>
	class Ring_buffer;
}


struct Genode::Ring_buffer_unsynchronized
{
	struct Sem
	{
		void down() { }
		void up() { }
	};

	struct Lock
	{
		void lock() { }
		void unlock() { }
	};

	struct Lock_guard
	{
		Lock_guard(Lock &) { }
	};
};


struct Genode::Ring_buffer_synchronized
{
	typedef Genode::Semaphore Sem;
	typedef Genode::Lock Lock;
	typedef Genode::Lock::Guard Lock_guard;
};


/**
 * Ring buffer template
 *
 * \param ET          element type
 * \param QUEUE_SIZE  number of element slots in the ring. the maximum number of
 *                    ring-buffer elements is QUEUE_SIZE - 1
 *
 * The ring buffer manages its elements as values.
 * When inserting an element, a copy of the element is
 * stored in the buffer. Hence, the ring buffer is suited
 * for simple plain-data element types.
 */
template <typename ET, int QUEUE_SIZE, typename SYNC_POLICY>
class Genode::Ring_buffer
{
	private:

		int _head = 0;
		int _tail = 0;

		typename SYNC_POLICY::Sem  _sem       { };  /* element counter */
		typename SYNC_POLICY::Lock _head_lock { };  /* synchronize add */

		ET _queue[QUEUE_SIZE] { };

	public:

		class Overflow : public Exception { };

		/**
		 * Constructor
		 */
		Ring_buffer() { }

		/**
		 * Place element into ring buffer
		 *
		 * \throw Overflow  the ring buffer is full
		 */
		void add(ET ev)
		{
			typename SYNC_POLICY::Lock_guard lock_guard(_head_lock);

			if ((_head + 1)%QUEUE_SIZE != _tail) {
				_queue[_head] = ev;
				_head = (_head + 1)%QUEUE_SIZE;
				_sem.up();
			} else
				throw Overflow();
		}

		/**
		 * Take element from ring buffer
		 *
		 * \return  element
		 *
		 * If the ring buffer is empty, this method blocks until an element
		 * gets available.
		 */
		ET get()
		{
			_sem.down();
			ET e = _queue[_tail];
			_tail = (_tail + 1)%QUEUE_SIZE;
			return e;
		}

		/**
		 * Return true if ring buffer is empty
		 */
		bool empty() const { return _tail == _head; }

		/**
		 * Return the remaining capacity
		 */
		int avail_capacity() const
		{
			if (_head >= _tail)
				return QUEUE_SIZE - _head + _tail - 1;
			else
				return _tail - _head - 1;
		}

		/**
		 * Discard all ring-buffer elements
		 */
		void reset() { _head = _tail; }
};

#endif /* _INCLUDE__OS__RING_BUFFER_H_ */
