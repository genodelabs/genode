/*
 * \brief  SBI-timer driver for RISC-V core
 * \author Sebastian Sumpf
 * \date   2021-01-29
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
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
	Hw::Riscv_cpu::Sie timer(STIE);
}


time_t Board::Timer::stime() const
{
	register time_t time asm("a0");
	asm volatile ("rdtime %0" : "=r"(time));

	return time;
}


void Timer::_start_one_shot(time_t const ticks)
{
	Sbi::set_timer(_time + ticks);
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return (ticks / Board::Timer::TICKS_PER_US); }


time_t Timer::us_to_ticks(time_t const us) const {
	return us * Board::Timer::TICKS_PER_US; }


time_t Timer::_max_value() const {
	return 0xffffffff; }


time_t Timer::_duration() const
{
	return _device.stime() - _time;
}


unsigned Timer::interrupt_id() const { return 5; }
