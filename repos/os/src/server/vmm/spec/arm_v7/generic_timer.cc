/*
 * \brief  VMM ARM Generic timer device model
 * \author Stefan Kalkowski
 * \date   2019-08-20
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>
#include <generic_timer.h>

using Vmm::Generic_timer;

Genode::uint64_t Generic_timer::_ticks_per_ms()
{
	static Genode::uint64_t ticks_per_ms = 0;
	if (!ticks_per_ms) {
		Genode::uint32_t freq = 0;
		asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r" (freq));
		ticks_per_ms = freq / 1000;
	}
	return ticks_per_ms;
}


Genode::uint64_t Generic_timer::_usecs_left()
{
	Genode::uint64_t count;
	Genode::uint32_t low, high;
	asm volatile("mrrc p15, 0, %0, %1, c14" : "=r" (low), "=r" (high));
	count = (Genode::uint64_t)high << 32 | (Genode::uint64_t)low;
	count -= _cpu.state().timer.offset;
	if (count > _cpu.state().timer.compare) return 0;
	return Genode::timer_ticks_to_us(_cpu.state().timer.compare - count,
	                                 _ticks_per_ms());
}
