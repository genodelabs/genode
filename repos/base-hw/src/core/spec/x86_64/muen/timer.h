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

#ifndef _TIMER_DRIVER_H_
#define _TIMER_DRIVER_H_

/* Genode includes */
#include <base/stdint.h>

namespace Board { class Timer; }


struct Board::Timer
{
	enum { TIMER_DISABLED = ~0ULL };

	Genode::uint64_t ticks_per_ms;
	Genode::uint64_t start;

	struct Subject_timed_event
	{
		Genode::uint64_t tsc_trigger;
		Genode::uint8_t  event_nr :6;
	} __attribute__((packed));

	struct Subject_timed_event * event_page = 0;
	struct Subject_timed_event * guest_event_page = 0;

	inline Genode::uint64_t rdtsc() const
	{
		Genode::uint32_t lo, hi;
		asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
		return (Genode::uint64_t)hi << 32 | lo;
	}

	class Invalid_region { };

	Timer(unsigned);
};

#endif /* _TIMER_DRIVER_H_ */
