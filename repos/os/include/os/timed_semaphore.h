/*
 * \brief  Semaphore implementation with timeout facility
 * \author Stefan Kalkowski
 * \date   2010-03-05
 *
 * This semaphore implementation allows to block on a semaphore for a
 * given time instead of blocking indefinetely.
 *
 * For the timeout functionality the alarm framework is used.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__TIMED_SEMAPHORE_H_
#define _INCLUDE__OS__TIMED_SEMAPHORE_H_

#include <base/thread.h>
#include <base/semaphore.h>
#include <timer_session/connection.h>
#include <os/alarm.h>

namespace Genode {

	class Timeout_thread;
	class Timed_semaphore;

	/**
	 * Exception types
	 */
	class Timeout_exception;
	class Nonblocking_exception;
}


/**
 * Alarm thread, which counts jiffies and triggers timeout events.
 */
class Genode::Timeout_thread : public  Thread_deprecated<2048*sizeof(long)>,
                               private Alarm_scheduler
{
	private:

		enum { JIFFIES_STEP_MS = 10 };

		static Genode::Env *_env;

		Timer::Connection   _timer    { *_env };
		Signal_context      _context  { };
		Signal_receiver     _receiver { };

		void entry(void);

	public:

		using Alarm_scheduler::schedule_absolute;
		using Alarm_scheduler::discard;

		Timeout_thread() : Thread_deprecated("alarm-timer")
		{
			_timer.sigh(_receiver.manage(&_context));
			_timer.trigger_periodic(JIFFIES_STEP_MS*1000);
			start();
		}

		Genode::Alarm::Time time(void) { return _timer.elapsed_ms(); }

		/*
		 * Returns the singleton timeout-thread used for all timeouts.
		 */
		static Timeout_thread *alarm_timer();

		static void env(Genode::Env &env) { _env = &env; }
};


class Genode::Timeout_exception     : public Exception { };
class Genode::Nonblocking_exception : public Exception { };


/**
 * Semaphore with timeout on down operation.
 */
class Genode::Timed_semaphore : public Semaphore
{
	private:

		typedef Semaphore::Element Element;

		/**
		 * Aborts blocking on the semaphore, raised when a timeout occured.
		 *
		 * \param  element the waiting-queue element associated with a timeout.
         * \return true if a thread was aborted/woken up
		 */
		bool _abort(Element &element)
		{
			Lock::Guard lock_guard(Semaphore::_meta_lock);

			/* potentially, the queue is empty */
			if (++Semaphore::_cnt <= 0) {

				/*
				 * Iterate through the queue and find the thread,
				 * with the corresponding timeout.
				 */
				Element *first = Semaphore::_queue.dequeue();
				Element *e     = first;

				while (true) {

					/*
					 * Wakeup the thread.
					 */
					if (&element == e) {
						e->wake_up();
						return true;
					}

					/*
					 * Noninvolved threads are enqueued again.
					 */
					Semaphore::_queue.enqueue(e);
					e = Semaphore::_queue.dequeue();

					/*
					 * Maybe, the alarm was triggered just after the corresponding
					 * thread was already dequeued, that's why we have to track
					 * whether we processed the whole queue.
					 */
					if (e == first)
						break;
				}
			}

			/* The right element was not found, so decrease counter again */
			--Semaphore::_cnt;
			return false;
		}


		/**
		 * Represents a timeout associated with the blocking
		 * operation on a semaphore.
		 */
		class Timeout : public Alarm
		{
			private:

				Timed_semaphore &_sem;       /* semaphore we block on */
				Element         &_element;   /* queue element timeout belongs to */
				bool             _triggered; /* timeout expired */
				Time             _start { };

			public:

				Timeout(Time duration, Timed_semaphore *s, Element *e)
				: _sem(*s), _element(*e), _triggered(false)
				{
					Timeout_thread *tt = Timeout_thread::alarm_timer();
					_start = tt->time();
					tt->schedule_absolute(this, _start + duration);
				}

				void discard(void)   { Timeout_thread::alarm_timer()->discard(this); }
				bool triggered(void) { return _triggered; }
				Time start()         { return _start;     }

			protected:

				bool on_alarm(unsigned) override
				{
					_triggered = _sem._abort(_element);
					return false;
				}
		};

	public:

		/**
		 * Constructor
		 *
		 * \param n  initial counter value of the semphore
		 */
		Timed_semaphore(int n = 0) : Semaphore(n) { }

		/**
		 * Decrements semaphore and blocks when it's already zero.
		 *
		 * \param t after t milliseconds of blocking a Timeout_exception is thrown.
		 *          if t is zero do not block, instead raise an
		 *          Nonblocking_exception.
		 * \return  milliseconds the caller was blocked
		 */
		Alarm::Time down(Alarm::Time t)
		{
			Semaphore::_meta_lock.lock();

			if (--Semaphore::_cnt < 0) {

				/* If t==0 we shall not block */
				if (t == 0) {
					++_cnt;
					Semaphore::_meta_lock.unlock();
					throw Genode::Nonblocking_exception();
				}

				/*
				 * Create semaphore queue element representing the thread
				 * in the wait queue.
				 */
				Element queue_element;
				Semaphore::_queue.enqueue(&queue_element);
				Semaphore::_meta_lock.unlock();

				/* Create the timeout */
				Timeout to(t, this, &queue_element);

				/*
				 * The thread is going to block on a local lock now,
				 * waiting for getting waked from another thread
				 * calling 'up()'
				 * */
				queue_element.block();

				/* Deactivate timeout */
				to.discard();

				/*
				 * When we were only woken up, because of a timeout,
				 * throw an exception.
				 */
				if (to.triggered())
					throw Genode::Timeout_exception();

				/* return blocking time */
				return Timeout_thread::alarm_timer()->time() - to.start();
			} else {
				Semaphore::_meta_lock.unlock();
			}
			return 0;
		}


		/********************************
		 ** Base class implementations **
		 ********************************/

		void down() { Semaphore::down(); }
		void up()   { Semaphore::up();   }
};

#endif /* _INCLUDE__OS__TIMED_SEMAPHORE_H_ */
