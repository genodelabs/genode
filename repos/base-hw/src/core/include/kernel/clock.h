/*
 * \brief   A clock manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__KERNEL__CLOCK_H_
#define _CORE__INCLUDE__KERNEL__CLOCK_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <util/list.h>

/* Core includes */
#include <timer.h>

namespace Kernel
{
	class Timeout;
	class Clock;
}

/**
 * A timeout causes a kernel pass and the call of a timeout specific handle
 */
class Kernel::Timeout : public Genode::List<Timeout>::Element
{
	friend class Clock;

	private:

		bool   _listed = false;
		time_t _start;
		time_t _end;
		bool   _end_period;

	public:

		/**
		 * Callback handle
		 */
		virtual void timeout_triggered() { }

		virtual ~Timeout() { }
};

/**
 * A clock manages a continuous time and timeouts on it
 */
class Kernel::Clock
{
	private:

		unsigned const        _cpu_id;
		Timer * const         _timer;
		time_t                _time = 0;
		bool                  _time_period = false;
		Genode::List<Timeout> _timeout_list[2];
		time_t                _last_timeout_duration = 0;

		bool _time_overflow(time_t const duration) const;

	public:

		Clock(unsigned const cpu_id, Timer * const timer);

		/**
		 * Set-up timer according to the current timeout schedule
		 */
		void schedule_timeout();

		/**
		 * Update time and work off expired timeouts
		 *
		 * \return  time that passed since the last scheduling
		 */
		time_t update_time();
		void process_timeouts();

		/**
		 * Set-up 'timeout' to trigger at time + 'duration'
		 */
		void set_timeout(Timeout * const timeout, time_t const duration);

		/**
		 * Return native time value that equals the given microseconds 'us'
		 */
		time_t us_to_tics(time_t const us) const;

		/**
		 * Return microseconds that passed since the last set-up of 'timeout'
		 */
		time_t timeout_age_us(Timeout const * const timeout) const;

		time_t timeout_max_us() const;
};

#endif /* _CORE__INCLUDE__KERNEL__CLOCK_H_ */
