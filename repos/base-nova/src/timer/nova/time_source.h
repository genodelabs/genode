/*
 * \brief  Time source using Nova timed semaphore down
 * \author Alexander Boettcher
 * \author Martin Stein
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		Genode::addr_t           _sem        { ~0UL };
		unsigned long            _timeout_us { 0 };
		unsigned long            _tsc_khz    { 0 };
		Duration                 _curr_time  { Microseconds(0) };
		Genode::Trace::Timestamp _tsc_start  { Genode::Trace::timestamp() };
		Genode::Trace::Timestamp _tsc_last   { _tsc_start };

		/* 1 / ((us / (1000 * 1000)) * (tsc_khz * 1000)) */
		enum { TSC_FACTOR = 1000ULL };

		inline Genode::uint64_t _tsc_to_us(Genode::uint64_t tsc) const
		{
			return (tsc) / (_tsc_khz / TSC_FACTOR);
		}


		/**************************
		 ** Threaded_time_source **
		 **************************/

		void _wait_for_irq();

	public:

		Time_source(Genode::Env &env);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		void schedule_timeout(Microseconds duration,
		                      Timeout_handler &handler) override;

		Microseconds max_timeout() const override
		{
			unsigned long long const max_us_ull = _tsc_to_us(~0ULL);
			return max_us_ull > ~0UL ? Microseconds(~0UL) : Microseconds(max_us_ull);
		}

		Duration curr_time() override
		{
			using namespace Genode::Trace;

			Timestamp    const curr_tsc = timestamp();
			Microseconds const diff(_tsc_to_us(curr_tsc - _tsc_last));

			/* update in irq context or if update rate is below 4000 irq/s */
			if (_irq || diff.value > 250) {
				_curr_time.add(diff);
				_tsc_last = curr_tsc;
			}

			return _curr_time;
		}
};

#endif /* _TIME_SOURCE_H_ */
