/*
 * \brief  Timer driver for core
 * \author Reto Buerki
 * \date   2015-04-14
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <base/printf.h>

/* core includes */
#include <board.h>
#include <sinfo.h>

namespace Genode
{
	/**
	 * Timer driver for core on Muen
	 */
	class Timer;
}

class Genode::Timer
{
	private:

		enum {
			TIMER_DISABLED = ~0ULL,
		};

		uint64_t _tics_per_ms;

		struct Subject_timer
		{
			uint64_t value;
			uint8_t  vector;
		} __attribute__((packed));

		struct Subject_timer * _timer_page;

		inline uint64_t rdtsc()
		{
			uint32_t lo, hi;
			asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
			return (uint64_t)hi << 32 | lo;
		}

		class Invalid_region {};

	public:

		Timer() : _tics_per_ms(Sinfo::get_tsc_khz())
		{
			struct Sinfo::Memregion_info region;
			if (!Sinfo::get_memregion_info("timer", &region)) {
				PERR("muen-timer: Unable to retrieve time memory region");
				throw Invalid_region();
			}

			_timer_page = (Subject_timer *)region.address;
			_timer_page->vector = Board::TIMER_VECTOR_KERNEL;
			PINF("muen-timer: page @0x%llx, frequency %llu kHz, vector %u",
			     region.address, _tics_per_ms, _timer_page->vector);
		}

		static unsigned interrupt_id(int)
		{
			return Board::TIMER_VECTOR_KERNEL;
		}

		inline void start_one_shot(uint32_t const tics, unsigned)
		{
			_timer_page->value = rdtsc() + tics;
		}

		uint32_t ms_to_tics(unsigned const ms)
		{
			return ms * _tics_per_ms;
		}

		unsigned value(unsigned)
		{
			const uint64_t now = rdtsc();
			if (_timer_page->value != TIMER_DISABLED
			    && _timer_page->value > now) {
				return _timer_page->value - now;
			}
			return 0;
		}

		/*
		 * Dummies
		 */
		static void disable_pit(void) { }
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
