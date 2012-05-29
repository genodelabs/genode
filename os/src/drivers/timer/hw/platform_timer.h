/*
 * \brief  Platform timer specific for base-hw
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__SRC__DRIVERS__TIMER__HW__PLATFORM_TIMER_H_
#define _OS__SRC__DRIVERS__TIMER__HW__PLATFORM_TIMER_H_

/* Genode includes */
#include <irq_session/connection.h>

/* Local includes */
#include <platform_timer_base.h>

/**
 * Platform timer specific for base-hw
 */
class Platform_timer : public Platform_timer_base,
                       public Genode::Irq_connection
{
	private:

		enum { MAX_TIMER_IRQS_PER_MS = 1 };

		unsigned long const _max_timeout_us; /* Maximum timeout in microsecs */
		unsigned long _curr_time_us; /* Accumulate already measured timeouts */
		unsigned long _init_value; /* Mark last processed timer value */
		Genode::Lock _update_curr_time_lock; /* Serialize curr_time access */

	public:

		/**
		 * Constructor
		 */
		Platform_timer() : Irq_connection(Platform_timer_base::IRQ),
		                   _max_timeout_us(tics_to_us(max_value())),
		                   _curr_time_us(0), _init_value(0)
		{ }

		/**
		 * Refresh and return our instance-own "now"-time in microseconds
		 *
		 * This function has to be executed regulary,
		 * at least all max_timeout() us.
		 */
		unsigned long curr_time()
		{
			/* Serialize updates on timeout counter */
			Genode::Lock::Guard lock(_update_curr_time_lock);

			/* Get time that passed since last time we've read the timer */
			bool wrapped;
			unsigned long const v = value(wrapped);
			unsigned long passed_time;
			if (wrapped) passed_time = _init_value + max_value() - v;
			else passed_time = _init_value - v;

			/* Update initial value for subsequent calculations */
			_init_value = v;

			/* Refresh our timeout counter and return it */
			_curr_time_us += tics_to_us(passed_time);
			return _curr_time_us;
		}

		/**
		 * Return maximum timeout as supported by the platform
		 */
		unsigned long max_timeout() const { return _max_timeout_us; }

		/**
		 * Schedule next timeout, oversized timeouts are truncated
		 *
		 * \param  timeout_us  Timeout in microseconds
		 */
		void schedule_timeout(unsigned long timeout_us)
		{
			/* Serialize updates on timeout counter */
			Genode::Lock::Guard lock(_update_curr_time_lock);

			/* Constraint timout value with our maximum IRQ rate
			 * and the maximum possible timout */
			if (timeout_us < 1000/MAX_TIMER_IRQS_PER_MS)
				timeout_us = 1000/MAX_TIMER_IRQS_PER_MS;
			if (timeout_us > _max_timeout_us)
				timeout_us = _max_timeout_us;

			/* Once the timer runs, one can wait for the its IRQ and update our
			 * timeout counter through 'curr_time()' (We rely on the fact that
			 * this is done at least one time in every max-timeout period) */
			_init_value = us_to_tics(timeout_us);
			run_and_wrap(_init_value);
		}

		/**
		 * Await the lastly scheduled timeout
		 */
		void wait_for_timeout(Genode::Thread_base *) { wait_for_irq(); }
};

#endif /* _OS__SRC__DRIVERS__TIMER__HW__PLATFORM_TIMER_H_ */

