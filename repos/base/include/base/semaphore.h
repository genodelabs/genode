/*
 * \brief  Semaphore
 * \author Norman Feske
 * \author Christian Prochaska
 * \date   2006-09-22
 */

/*
 * Copyright (C) 2006-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SEMAPHORE_H_
#define _INCLUDE__BASE__SEMAPHORE_H_

#include <base/blockade.h>
#include <base/mutex.h>
#include <util/fifo.h>

namespace Genode { class Semaphore; }


class Genode::Semaphore
{
	protected:

		int   _cnt;
		Mutex _meta_lock { };

		struct Element : Fifo<Element>::Element { Blockade blockade { }; };

		Fifo<Element> _queue { };

	public:

		/**
		 * Constructor
		 *
		 * \param n  initial counter value of the semphore
		 */
		Semaphore(int n = 0) : _cnt(n) { }

		~Semaphore()
		{
			/* synchronize destruction with unfinished 'up()' */
			try { _meta_lock.acquire(); } catch (...) { }
		}

		/**
		 * Increment semphore counter
		 *
		 * This method may wake up another thread that currently blocks on
		 * a 'down' call at the same semaphore.
		 */
		void up()
		{
			Element * element = nullptr;

			{
				Mutex::Guard guard(_meta_lock);

				if (++_cnt > 0)
					return;

				/*
				 * Remove element from queue and wake up the corresponding
				 * blocking thread
				 */
				_queue.dequeue([&element] (Element &head) {
					element = &head; });
			}

			/* do not hold the lock while unblocking a waiting thread */
			if (element) element->blockade.wakeup();
		}

		/**
		 * Decrement semaphore counter, block if the counter reaches zero
		 */
		void down()
		{
			_meta_lock.acquire();

			if (--_cnt < 0) {

				/*
				 * Create semaphore queue element representing the thread
				 * in the wait queue.
				 */
				Element queue_element;
				_queue.enqueue(queue_element);
				_meta_lock.release();

				/*
				 * The thread is going to block on a local lock now,
				 * waiting for getting waked from another thread
				 * calling 'up()'
				 * */
				queue_element.blockade.block();

			} else {
				_meta_lock.release();
			}
		}

		/**
		 * Return current semaphore counter
		 */
		int cnt() { return _cnt; }
};

#endif /* _INCLUDE__BASE__SEMAPHORE_H_ */
