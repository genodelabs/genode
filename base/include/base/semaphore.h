/*
 * \brief  Semaphore
 * \author Norman Feske
 * \author Christian Prochaska
 * \date   2006-09-22
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SEMAPHORE_H_
#define _INCLUDE__BASE__SEMAPHORE_H_

#include <base/lock.h>
#include <util/list.h>
#include <util/fifo.h>

namespace Genode {

	/**
	 * Semaphore queue interface
	 */
	class Semaphore_queue
	{
		public:

			/**
			 * Semaphore-queue elements
			 *
			 * A queue element represents a thread blocking on the
			 * semaphore.
			 */
			class Element : Lock
			{
				public:

					/**
					 * Constructor
					 */
					Element() : Lock(LOCKED) { }

					void block()   { lock();   }
					void wake_up() { unlock(); }
			};

			/**
			 * Add new queue member that is going to block
			 */
			void enqueue(Element *e);

			/**
			 * Dequeue queue member to wake up next
			 */
			Element *dequeue();
	};


	/**
	 * First-in-first-out variant of the semaphore-queue interface
	 */
	class Fifo_semaphore_queue : public Semaphore_queue
	{
		public:

			class Element : public Semaphore_queue::Element,
			                public Fifo<Element>::Element { };

		private:

			Fifo<Element> _fifo;

		public:

			void enqueue(Element *e) { _fifo.enqueue(e); }

			Element *dequeue()  { return _fifo.dequeue(); }
	};


	/**
	 * Semaphore base template
	 *
	 * \param QT  semaphore wait queue type implementing the
	 *            'Semaphore_queue' interface
	 * \param QTE wait-queue element type implementing the
	 *            'Semaphore_queue::Element' interface
	 *
	 * The queuing policy is defined via the QT and QTE types.
	 * This way, the platform-specific semaphore-queueing policies
	 * such as priority-sorted queueing can be easily supported.
	 */
	template <typename QT, typename QTE>
	class Semaphore_template
	{
		protected:

			int  _cnt;
			Lock _meta_lock;
			QT   _queue;

		public:

			/**
			 * Constructor
			 *
			 * \param n  initial counter value of the semphore
			 */
			Semaphore_template(int n = 0) : _cnt(n) { }

			~Semaphore_template()
			{
				/* synchronize destruction with unfinished 'up()' */
				try { _meta_lock.lock(); } catch (...) { }
			}

			void up()
			{
				Lock::Guard lock_guard(_meta_lock);

				if (++_cnt > 0)
					return;

				/*
				 * Remove element from queue and wake up the corresponding
				 * blocking thread
				 */
				Semaphore_queue::Element * element = _queue.dequeue();
				if (element)
					element->wake_up();
			}

			void down()
			{
				_meta_lock.lock();

				if (--_cnt < 0) {

					/*
					 * Create semaphore queue element representing the thread
					 * in the wait queue.
					 */
					QTE queue_element;
					_queue.enqueue(&queue_element);
					_meta_lock.unlock();

					/*
					 * The thread is going to block on a local lock now,
					 * waiting for getting waked from another thread
					 * calling 'up()'
					 * */
					queue_element.block();

				} else {
					_meta_lock.unlock();
				}
			}

			/**
			 * Return current semaphore counter
			 */
			int cnt() { return _cnt; }
	};


	/**
	 * Semaphore with default behaviour
	 */
	typedef Semaphore_template<Fifo_semaphore_queue, Fifo_semaphore_queue::Element> Semaphore;
}

#endif /* _INCLUDE__BASE__SEMAPHORE_H_ */
