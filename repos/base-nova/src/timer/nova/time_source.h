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
#include <base/attached_rom_dataspace.h>
#include <base/log.h>

/* local includes */
#include <threaded_time_source.h>

namespace Timer {

	using Genode::uint64_t;
	class Time_source;
}


class Timer::Time_source : public Threaded_time_source
{
	private:

		/* read the tsc frequency from platform info */
		static unsigned long _obtain_tsc_khz(Genode::Env &env)
		{
			unsigned long result = 0;
			try {
				Genode::Attached_rom_dataspace info { env, "platform_info"};

				result = info.xml().sub_node("hardware")
				                   .sub_node("tsc")
				                   .attribute_value("freq_khz", 0UL);
			} catch (...) { }

			if (result)
				return result;

			/*
			 * The returned value must never be zero because it is used as
			 * divisor by '_tsc_to_us'.
			 */
			Genode::warning("unable to obtain tsc frequency, asuming 1 GHz");
			return 1000*1000;
		}

		Genode::addr_t           _sem        { ~0UL };
		uint64_t                 _timeout_us { 0 };
		unsigned long      const _tsc_khz;
		Duration                 _curr_time  { Microseconds(0) };
		Genode::Trace::Timestamp _tsc_start  { Genode::Trace::timestamp() };
		Genode::Trace::Timestamp _tsc_last   { _tsc_start };

		/* 1 / ((us / (1000 * 1000)) * (tsc_khz * 1000)) */
		enum { TSC_FACTOR = 1000ULL };

		inline uint64_t _tsc_to_us(uint64_t tsc) const
		{
			return (tsc) / (_tsc_khz / TSC_FACTOR);
		}


		/**************************
		 ** Threaded_time_source **
		 **************************/

		void _wait_for_irq() override;

	public:

		Time_source(Genode::Env &env)
		:
			Threaded_time_source(env), _tsc_khz(_obtain_tsc_khz(env))
		{
			start();
		}

		/*************************
		 ** Genode::Time_source **
		 *************************/

		void schedule_timeout(Microseconds duration,
		                      Timeout_handler &handler) override;

		Microseconds max_timeout() const override
		{
			uint64_t const max_us = _tsc_to_us(~(uint64_t)0);
			return max_us > ~(uint64_t)0 ? Microseconds(~(uint64_t)0) : Microseconds(max_us);
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
