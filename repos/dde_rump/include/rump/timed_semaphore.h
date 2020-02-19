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

#ifndef _INCLUDE__RUMP__TIMED_SEMAPHORE_H_
#define _INCLUDE__RUMP__TIMED_SEMAPHORE_H_

#include <base/thread.h>
#include <base/semaphore.h>
#include <base/alarm.h>
#include <timer_session/connection.h>

using Genode::Exception;
using Genode::Entrypoint;
using Genode::Alarm;
using Genode::Alarm_scheduler;
using Genode::Semaphore;
using Genode::Signal_handler;

/**
 * Exception types
 */
class Timeout_exception     : public Exception { };
class Nonblocking_exception : public Exception { };


/**
 * Alarm thread, which counts jiffies and triggers timeout events.
 */
class Timeout_entrypoint : private Entrypoint
{
	private:

		enum { JIFFIES_STEP_MS = 10 };

		Alarm_scheduler _alarm_scheduler { };

		Timer::Connection _timer;

		Signal_handler<Timeout_entrypoint> _timer_handler;

		void _handle_timer() { _alarm_scheduler.handle(_timer.elapsed_ms()); }

		static Genode::size_t constexpr STACK_SIZE = 2048*sizeof(long);

	public:

		Timeout_entrypoint(Genode::Env &env)
		:
			Entrypoint(env, STACK_SIZE, "alarm-timer", Genode::Affinity::Location()),
			_timer(env),
			_timer_handler(*this, *this, &Timeout_entrypoint::_handle_timer)
		{
			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(JIFFIES_STEP_MS*1000);
		}

		Alarm::Time time(void) { return _timer.elapsed_ms(); }

		void schedule_absolute(Alarm &alarm, Alarm::Time timeout)
		{
			_alarm_scheduler.schedule_absolute(&alarm, timeout);
		}

		void discard(Alarm &alarm) { _alarm_scheduler.discard(&alarm); }
};


/**
 * Semaphore with timeout on down operation.
 */
class Timed_semaphore : public Semaphore
{
	private:

		typedef Semaphore::Element Element;

		Timeout_entrypoint &_timeout_ep;

		/**
		 * Aborts blocking on the semaphore, raised when a timeout occured.
		 *
		 * \param  element the waiting-queue element associated with a timeout.
		 * \return true if a thread was aborted/woken up
		 */
		bool _abort(Element &element)
		{
			Genode::Mutex::Guard lock_guard(Semaphore::_meta_lock);

			/* potentially, the queue is empty */
			if (++Semaphore::_cnt <= 0) {

				/*
				 * Iterate through the queue and find the thread,
				 * with the corresponding timeout.
				 */
				Element *first = nullptr;
				Semaphore::_queue.dequeue([&first] (Element &e) {
					first = &e; });
				Element *e     = first;

				while (e) {

					/*
					 * Wakeup the thread.
					 */
					if (&element == e) {
						e->blockade.wakeup();
						return true;
					}

					/*
					 * Noninvolved threads are enqueued again.
					 */
					Semaphore::_queue.enqueue(*e);
					e = nullptr;
					Semaphore::_queue.dequeue([&e] (Element &next) {
						e = &next; });

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

				Timed_semaphore &_sem;      /* semaphore we block on */
				Element         &_element;  /* queue element timeout belongs to */
				bool             _triggered { false };
				Time       const _start;

			public:

				Timeout(Time start, Timed_semaphore &s, Element &e)
				: _sem(s), _element(e), _triggered(false), _start(start)
				{ }

				bool triggered(void) { return _triggered; }
				Time start()         { return _start;     }

			protected:

				bool on_alarm(Genode::uint64_t) override
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
		Timed_semaphore(Timeout_entrypoint &timeout_ep, int n = 0)
		: Semaphore(n), _timeout_ep(timeout_ep) { }

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
			Semaphore::_meta_lock.acquire();

			if (--Semaphore::_cnt < 0) {

				/* If t==0 we shall not block */
				if (t == 0) {
					++_cnt;
					Semaphore::_meta_lock.release();
					throw Nonblocking_exception();
				}

				/*
				 * Create semaphore queue element representing the thread
				 * in the wait queue.
				 */
				Element queue_element;
				Semaphore::_queue.enqueue(queue_element);
				Semaphore::_meta_lock.release();

				/* Create the timeout */
				Alarm::Time const curr_time = _timeout_ep.time();
				Timeout timeout(curr_time, *this, queue_element);
				_timeout_ep.schedule_absolute(timeout, curr_time + t);

				/*
				 * The thread is going to block on a local lock now,
				 * waiting for getting waked from another thread
				 * calling 'up()'
				 * */
				queue_element.blockade.block();

				/* Deactivate timeout */
				_timeout_ep.discard(timeout);

				/*
				 * When we were only woken up, because of a timeout,
				 * throw an exception.
				 */
				if (timeout.triggered())
					throw Timeout_exception();

				/* return blocking time */
				return _timeout_ep.time() - timeout.start();

			} else {
				Semaphore::_meta_lock.release();
			}
			return 0;
		}


		/********************************
		 ** Base class implementations **
		 ********************************/

		void down() { Semaphore::down(); }
		void up()   { Semaphore::up();   }
};

#endif /* _INCLUDE__RUMP__TIMED_SEMAPHORE_H_ */
