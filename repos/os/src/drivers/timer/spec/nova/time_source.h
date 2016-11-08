/*
 * \brief  Time source using Nova timed semaphore down
 * \author Alexander Boettcher
 * \author Martin Stein
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIME_SOURCE_H_
#define _TIME_SOURCE_H_

/* Genode includes */
#include <trace/timestamp.h>

/* local includes */
#include <threaded_time_source.h>

namespace Timer { class Time_source; }


class Timer::Time_source : public Threaded_time_source
{
	private:

		Genode::addr_t           _sem = ~0UL;
		unsigned long            _timeout_us = 0;
		Genode::Trace::Timestamp _tsc_start = Genode::Trace::timestamp();
		unsigned long            _tsc_khz;

		/* 1 / ((us / (1000 * 1000)) * (tsc_khz * 1000)) */
		enum { TSC_FACTOR = 1000ULL };

		inline Microseconds _tsc_to_us(unsigned long long tsc,
		                               bool sub_tsc_start = true) const
		{
			if (sub_tsc_start) {
				return Microseconds((tsc - _tsc_start) /
				                    (_tsc_khz / TSC_FACTOR));
			}
			return Microseconds((tsc) / (_tsc_khz / TSC_FACTOR));
		}


		/**************************
		 ** Threaded_time_source **
		 **************************/

		void _wait_for_irq();

	public:

		Time_source(Genode::Entrypoint &ep);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Microseconds max_timeout() const override { return _tsc_to_us(~0UL); }
		void schedule_timeout(Microseconds duration, Timeout_handler &handler) override;
		Microseconds curr_time() const override {
			return _tsc_to_us(Genode::Trace::timestamp()); }
};

#endif /* _TIME_SOURCE_H_ */
