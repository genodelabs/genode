/*
 * \brief  Queue for deferred command execution
 * \author Johannes Schlatow
 * \date   2025-11-14
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _QUEUE_H_
#define _QUEUE_H_

namespace Window_layouter
{
	template <typename, int>
	class Queue;
}


/**
 * Queue template
 *
 * \param ET          element type
 * \param QUEUE_SIZE  number of slots in the queue. maximum number of
 *                    elements is QUEUE_SIZE - 1
 */
template <typename ET, int QUEUE_SIZE>
class Window_layouter::Queue
{
	private:

		int _head = 0;
		int _tail = 0;

		ET _queue[QUEUE_SIZE] { };

	public:

		void enqueue(ET ev)
		{
			if ((_head + 1)%QUEUE_SIZE != _tail) {
				_queue[_head] = ev;
				_head = (_head + 1)%QUEUE_SIZE;
			}
		}

		template <typename FN>
		bool dequeue_if(FN && fn)
		{
			if (empty())
				return false;

			if (!fn(_queue[_tail]))
				return false;

			_tail = (_tail + 1)%QUEUE_SIZE;
			return true;
		}

		bool empty() const { return _tail == _head; }

		int avail_capacity() const
		{
			if (_head >= _tail)
				return QUEUE_SIZE - _head + _tail - 1;
			else
				return _tail - _head - 1;
		}

		void reset() { _head = _tail; }
};

#endif /* _QUEUE_H_ */
