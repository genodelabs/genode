/*
 * \brief  Platform timer based on spinning usleep
 * \author Norman Feske
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_TIMER_H_
#define _PLATFORM_TIMER_H_

/* Genode inludes */
#include <base/thread.h>
#include <os/server.h>


class Platform_timer
{
	private:

		Genode::Lock mutable _lock;               /* for protecting '_next_timeout_usec' */
		unsigned long        _next_timeout_usec;  /* timeout of current sleep */
		unsigned long        _curr_time_usec;     /* accumulated sleep time */

		/**
		 * Platform-specific sleep implementation
		 */
		void _usleep(unsigned long usecs);

	public:

		/**
		 * Constructor
		 */
		Platform_timer() : _next_timeout_usec(max_timeout()), _curr_time_usec(0) { }

		/**
		 * Set next relative timeout
		 */
		void schedule_timeout(unsigned long timeout_usec)
		{
			Genode::Lock::Guard lock_guard(_lock);
			_next_timeout_usec = timeout_usec;
		}

		/**
		 * Return maximum supported timeout in microseconds
		 */
		unsigned long max_timeout();

		/**
		 * Get current time in microseconds
		 */
		unsigned long curr_time() const;

		/**
		 * Block until the scheduled timeout triggers
		 */
		void wait_for_timeout(Genode::Thread_base *blocking_thread)
		{
			enum { SLEEP_GRANULARITY_USEC = 1000UL };

			unsigned long last_time = curr_time();
			_lock.lock();
			while (_next_timeout_usec) {
				_lock.unlock();

				try { _usleep(SLEEP_GRANULARITY_USEC); }
				catch (Genode::Blocking_canceled) { }

				unsigned long now_time       = curr_time();
				unsigned long sleep_duration = now_time - last_time;
				last_time = now_time;

				_lock.lock();

				if (_next_timeout_usec >= sleep_duration)
					_next_timeout_usec -= sleep_duration;
				else
					break;
			}
			_lock.unlock();
		}
};

#endif /* _PLATFORM_TIMER_H_ */
