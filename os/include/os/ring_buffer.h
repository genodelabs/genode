/*
 * \brief  Ring buffer
 * \author Norman Feske
 * \date   2007-09-28
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__RING_BUFFER_H_
#define _INCLUDE__OS__RING_BUFFER_H_

#include <base/semaphore.h>
#include <base/exception.h>
#include <util/string.h>

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
template <typename ET, int QUEUE_SIZE>
class Ring_buffer
{
	private:

		int               _head;
		int               _tail;
		Genode::Semaphore _sem;        /* element counter */
		Genode::Lock      _head_lock;  /* synchronize add */

		ET                _queue[QUEUE_SIZE];

	public:

		class Overflow : public Genode::Exception { };

		/**
		 * Constructor
		 */
		Ring_buffer(): _head(0), _tail(0) {
			Genode::memset(_queue, 0, sizeof(_queue)); }

		/**
		 * Place element into ring buffer
		 *
		 * If the ring buffer is full, this function
		 * throws an Overflow exception.
		 */
		void add(ET ev)
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
		bool empty() { return _tail == _head; }
};

#endif /* _INCLUDE__OS__RING_BUFFER_H_ */
