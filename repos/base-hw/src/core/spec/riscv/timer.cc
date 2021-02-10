/*
 * \brief  SBI-timer driver for RISC-V core
 * \author Sebastian Sumpf
 * \date   2021-01-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/timer.h>
#include <platform.h>
#include <hw/spec/riscv/sbi.h>

using namespace Genode;
using namespace Kernel;


Board::Timer::Timer(unsigned)
{
	/* enable timer interrupt */
	enum { STIE = 0x20 };
	asm volatile ("csrs sie, %0" : : "r"(STIE));
}


time_t Board::Timer::stime() const
{
	register time_t time asm("a0");
	asm volatile ("rdtime %0" : "=r"(time));

	return time;
}


void Timer::_start_one_shot(time_t const ticks)
{
	_device.timeout = _device.stime() + ticks;
	Sbi::set_timer(_device.timeout);
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return (ticks / Board::Timer::TICKS_PER_US); }


time_t Timer::us_to_ticks(time_t const us) const {
	return us * Board::Timer::TICKS_PER_US; }


time_t Timer::_max_value() const {
	return 0xffffffff; }


time_t Timer::_duration() const
{
	addr_t time = _device.stime();
	return time < _device.timeout ? _device.timeout - time
	                              : _last_timeout_duration + (time - _device.timeout);
}


unsigned Timer::interrupt_id() const { return 5; }
