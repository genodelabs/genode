/*
 * \brief  Platform timer specific for base-hw
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__SRC__DRIVERS__TIMER__HW__PLATFORM_TIMER_H_
#define _OS__SRC__DRIVERS__TIMER__HW__PLATFORM_TIMER_H_

/* Genode includes */
#include <base/signal.h>
#include <os/server.h>

/* base-hw includes */
#include <kernel/interface.h>

/**
 * Platform timer specific for base-hw
 */
class Platform_timer
{
	private:

		using time_t = Kernel::time_t;

		Genode::Signal_receiver _sigrec;
		Genode::Signal_context  _sigctx;
		Kernel::capid_t const   _sigid;
		unsigned long mutable   _curr_time_us;
		Genode::Lock  mutable   _curr_time_us_lock;
		unsigned long mutable   _last_timeout_us;
		time_t const            _max_timeout_us;

		/**
		 * Return kernel capability selector of Genode capability
		 *
		 * This function is normally framework-internal and defined in
		 * 'base/internal/capability_space.h'.
		 */
		static inline Kernel::capid_t _capid(Genode::Native_capability const &cap)
		{
			Genode::addr_t const index = (Genode::addr_t)cap.data();
			return index;
		}

	public:

		Platform_timer()
		:
			_sigid(_capid(_sigrec.manage(&_sigctx))),
			_curr_time_us(0), _last_timeout_us(0),
			_max_timeout_us(Kernel::timeout_max_us())
		{
			Genode::log("maximum timeout ", _max_timeout_us, " us");
			if (max_timeout() < min_timeout()) {
				Genode::error("minimum timeout greater then maximum timeout");
				throw Genode::Exception();
			}
		}

		~Platform_timer() { _sigrec.dissolve(&_sigctx); }

		/**
		 * Refresh and return our instance-own "now"-time in microseconds
		 *
		 * This function has to be executed regulary, at least all
		 * max_timeout() us.
		 */
		unsigned long curr_time() const
		{
			Genode::Lock::Guard lock(_curr_time_us_lock);
			time_t const passed_us = Kernel::timeout_age_us();
			_last_timeout_us -= passed_us;
			_curr_time_us += passed_us;
			return _curr_time_us;
		}

		/**
		 * Return maximum timeout in microseconds
		 */
		time_t max_timeout() const { return _max_timeout_us; }

		/**
		 * Return minimum timeout in microseconds
		 */
		static time_t min_timeout() { return 1000; }

		/**
		 * Schedule next timeout, bad timeouts are adapted
		 *
		 * \param  timeout_us  Timeout in microseconds
		 */
		void schedule_timeout(time_t timeout_us)
		{
			Genode::Lock::Guard lock(_curr_time_us_lock);
			if (timeout_us < min_timeout()) { timeout_us = min_timeout(); }
			if (timeout_us > max_timeout()) { timeout_us = max_timeout(); }

			/*
			 * Once the timer runs, one can wait for its signal and update our
			 * timeout counter through 'curr_time()' (We rely on the fact that
			 * this is done at least one time in every max-timeout period)
			 */
			_last_timeout_us = timeout_us;
			Kernel::timeout(timeout_us, _sigid);
		}

		/**
		 * Await the lastly scheduled timeout
		 */
		void wait_for_timeout(Genode::Thread *) { _sigrec.wait_for_signal(); }
};

#endif /* _OS__SRC__DRIVERS__TIMER__HW__PLATFORM_TIMER_H_ */
