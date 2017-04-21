/*
 * \brief   A timer manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__TIMER_H_
#define _CORE__KERNEL__TIMER_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <util/list.h>

/* Core includes */
#include <timer_driver.h>

namespace Kernel
{
	class Timeout;
	class Timer;
}

/**
 * A timeout causes a kernel pass and the call of a timeout specific handle
 */
class Kernel::Timeout : public Genode::List<Timeout>::Element
{
	friend class Timer;

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
 * A timer manages a continuous time and timeouts on it
 */
class Kernel::Timer
{
	private:

		using Driver = Timer_driver;

		unsigned const        _cpu_id;
		Driver                _driver;
		time_t                _time = 0;
		bool                  _time_period = false;
		Genode::List<Timeout> _timeout_list[2];
		time_t                _last_timeout_duration = 0;

		bool _time_overflow(time_t const duration) const;

		void _start_one_shot(time_t const ticks);

		time_t _ticks_to_us(time_t const ticks) const;

		time_t _value();

		time_t _max_value() const;

	public:

		Timer(unsigned cpu_id);

		void schedule_timeout();

		time_t update_time();
		void process_timeouts();

		void set_timeout(Timeout * const timeout, time_t const duration);

		time_t us_to_ticks(time_t const us) const;

		time_t timeout_age_us(Timeout const * const timeout) const;

		time_t timeout_max_us() const;

		unsigned interrupt_id() const;

		static void init_cpu_local();

		time_t time() const { return _time; }
};

#endif /* _CORE__KERNEL__TIMER_H_ */
