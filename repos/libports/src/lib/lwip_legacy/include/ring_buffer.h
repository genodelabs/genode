/*
 * \brief  Ring buffer implementation.
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-10-29
 *
 * This ring buffer implementation is taken from the os repository.
 * In contrast to the original implementation this one lets pass
 * timeouts.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__INCLUDE__RING_BUFFER_H__
#define __LWIP__INCLUDE__RING_BUFFER_H__

#include <base/exception.h>
#include <util/string.h>

#include <os/timed_semaphore.h>

namespace Lwip {

/**
 * Ring buffer
 *
 * The ring buffer manages its elements as values.
 * When inserting an element, a copy of the element is
 * stored in the buffer. Hence, the ring buffer is suited
 * for simple plain-data element types.
 */
class Ring_buffer
{
	private:

		enum Constants { QUEUE_SIZE=128 };

		int                     _head;
		int                     _tail;
		Genode::Timed_semaphore _sem;        /* element counter */
		Genode::Lock            _head_lock;  /* synchronize add */

		void*                   _queue[QUEUE_SIZE];

	public:

		class Overflow : public Genode::Exception { };

		static const Genode::Alarm::Time NO_BLOCK = 1;

		/**
		 * Constructor
		 */
		Ring_buffer() : _head(0), _tail(0)
		{
			Genode::memset(_queue, 0, sizeof(_queue));
		}

		/**
		 * Place element into ring buffer
		 *
		 * If the ring buffer is full, this function
		 * throws an Overflow exception.
		 */
		void add(void* ev)
		{
			Genode::Lock::Guard lock_guard(_head_lock);

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
		 * If the ring buffer is empty, this function
		 * blocks until an element gets available.
		 */
		Genode::Alarm::Time get(void* *ev, Genode::Alarm::Time t)
		{
			Genode::Alarm::Time time;

			if (t == NO_BLOCK) {
				time = _sem.down(0);
			} else if (t == 0) {
				time = Genode::Timeout_thread::alarm_timer()->time();
				_sem.down();
				time = Genode::Timeout_thread::alarm_timer()->time() - time;
			} else {
				time = _sem.down(t);
			}

			Genode::Lock::Guard lock_guard(_head_lock);

			*ev   = _queue[_tail];
			_tail = (_tail + 1)%QUEUE_SIZE;
			return time;
		}

		/**
		 * Return true if ring buffer is empty
		 */
		bool empty() { return _tail == _head; }
};

}

#endif //__LWIP__INCLUDE__RING_BUFFER_H_
