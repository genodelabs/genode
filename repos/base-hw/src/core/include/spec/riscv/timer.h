/*
 * \brief  Timer driver for core
 * \author Sebastian Sumpf
 * \date   2015-08-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <base/stdint.h>

/* Core includes */
#include <machine_call.h>

namespace Genode { class Timer; }

/**
 * Timer driver for core
 */
struct Genode::Timer
{
	private:

		using time_t = Kernel::time_t;

		enum {
			SPIKE_TIMER_HZ = 500000,
			TICS_PER_MS    = SPIKE_TIMER_HZ / 1000,
		};

		addr_t _timeout = 0;

		addr_t _stime()
		{
			addr_t t;
			asm volatile ("csrr %0, stime\n" : "=r"(t));
			return t;
		}

	public:

		Timer()
		{
			/* enable timer interrupt */
			enum { STIE = 0x20 };
			asm volatile ("csrs sie, %0" : : "r"(STIE));
		}

		/**
		 * Start single timeout run
		 *
		 * \param tics  delay of timer interrupt
		 */
		void start_one_shot(time_t const tics, unsigned const)
		{
			_timeout = _stime() + tics;
			Machine::set_sys_timer(_timeout);
		}

		time_t tics_to_us(time_t const tics) const {
			return (tics / TICS_PER_MS) * 1000; }

		time_t us_to_tics(time_t const us) const {
			return (us / 1000) * TICS_PER_MS; }

		time_t max_value() { return (addr_t)~0; }

		/**
		 * Return current native timer value
		 */
		time_t value(unsigned const)
		{
			addr_t time = _stime();
			return time < _timeout ? _timeout - time : 0;
		}

		static unsigned interrupt_id(unsigned const) { return 1; }
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
