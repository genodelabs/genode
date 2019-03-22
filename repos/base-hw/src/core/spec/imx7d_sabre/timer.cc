/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \author Martin stein
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <drivers/timer/util.h>
#include <kernel/timer.h>
#include <board.h>
#include <platform.h>

using namespace Genode;
using namespace Kernel;


unsigned Timer::interrupt_id() const
{
	return 30;
}

unsigned long Timer_driver::_freq()
{
	unsigned long freq;
	asm volatile("mrc p15, 0, %0, c14, c0, 0\n" : "=r" (freq));
	return freq;
}


Timer_driver::Timer_driver(unsigned) : ticks_per_ms(_freq() / 1000)
{
	unsigned long ctrl;
	asm volatile("mrc p15, 0, %0, c14, c2, 1\n" : "=r" (ctrl));
	asm volatile("mcr p15, 0, %0, c14, c2, 1\n" :: "r" (ctrl | 1));
}


void Timer::_start_one_shot(time_t const ticks)
{
	unsigned long ctrl;
	unsigned long v0, v1;
	asm volatile("mrrc p15, 0, %0, %1, c14\n" : "=r" (v0), "=r" (v1));
	_driver.last_time = (Genode::uint64_t)v0 | (Genode::uint64_t)v1 << 32;
	asm volatile("mcr p15, 0, %0, c14, c2, 0\n" :: "r" (ticks));
	asm volatile("mrc p15, 0, %0, c14, c2, 1\n" : "=r" (ctrl));
	asm volatile("mcr p15, 0, %0, c14, c2, 1\n" :: "r" (ctrl & ~4UL));
}


time_t Timer::_duration() const
{
	unsigned long v0, v1;
	asm volatile("mrrc p15, 0, %0, %1, c14\n" : "=r" (v0), "=r" (v1));
	Genode::uint64_t v = (Genode::uint64_t)v0 | (Genode::uint64_t)v1 << 32;
	return v - _driver.last_time;
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, _driver.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * _driver.ticks_per_ms; }


time_t Timer::_max_value() const {
	return _driver.ticks_per_ms * 5000; }
