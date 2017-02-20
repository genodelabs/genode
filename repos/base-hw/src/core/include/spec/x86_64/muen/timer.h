/*
 * \brief  Timer driver for core
 * \author Reto Buerki
 * \date   2015-04-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__MUEN__TIMER_H_
#define _CORE__INCLUDE__SPEC__X86_64__MUEN__TIMER_H_

/* base includes */
#include <base/log.h>
#include <kernel/types.h>

/* core includes */
#include <board.h>
#include <sinfo_instance.h>

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

		using time_t = Kernel::time_t;

		enum { TIMER_DISABLED = ~0ULL };

		uint64_t _tics_per_ms;

		struct Subject_timed_event
		{
			uint64_t tsc_trigger;
			uint8_t  event_nr :5;
		} __attribute__((packed));

		struct Subject_timed_event * _event_page = 0;
		struct Subject_timed_event * _guest_event_page = 0;


		inline uint64_t rdtsc()
		{
			uint32_t lo, hi;
			asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
			return (uint64_t)hi << 32 | lo;
		}

		class Invalid_region {};

	public:

		Timer() :
			_tics_per_ms(sinfo()->get_tsc_khz())
		{

			/* first sinfo instance, output status */
			sinfo()->log_status();

			struct Sinfo::Memregion_info region;
			if (!sinfo()->get_memregion_info("timed_event", &region)) {
				error("muen-timer: Unable to retrieve timed event region");
				throw Invalid_region();
			}

			_event_page = (Subject_timed_event *)Platform::mmio_to_virt(region.address);
			_event_page->event_nr = Board::TIMER_EVENT_KERNEL;
			log("muen-timer: Page @", Hex(region.address), ", "
			    "frequency ", _tics_per_ms, " kHz, "
			    "event ", (unsigned)_event_page->event_nr);

			if (sinfo()->get_memregion_info("monitor_timed_event", &region)) {
				log("muen-timer: Found guest timed event page @", Hex(region.address),
				    " -> enabling preemption");
				_guest_event_page = (Subject_timed_event *)Platform::mmio_to_virt(region.address);
				_guest_event_page->event_nr = Board::TIMER_EVENT_PREEMPT;
			}
		}

		static unsigned interrupt_id(int) {
			return Board::TIMER_VECTOR_KERNEL; }

		inline void start_one_shot(time_t const tics, unsigned)
		{
			const uint64_t t = rdtsc() + tics;
			_event_page->tsc_trigger = t;

			if (_guest_event_page)
				_guest_event_page->tsc_trigger = t;
		}

		time_t tics_to_us(time_t const tics) const {
			return (tics / _tics_per_ms) * 1000; }

		time_t us_to_tics(time_t const us) const {
			return (us / 1000) * _tics_per_ms; }

		time_t max_value() { return (time_t)~0; }

		time_t value(unsigned)
		{
			const uint64_t now = rdtsc();
			if (_event_page->tsc_trigger != TIMER_DISABLED
			    && _event_page->tsc_trigger > now) {
				return _event_page->tsc_trigger - now;
			}
			return 0;
		}

		static void disable_pit(void) { }
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _CORE__INCLUDE__SPEC__X86_64__MUEN__TIMER_H_ */
