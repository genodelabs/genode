/*
 * \brief  Timer driver for core
 * \author Sebastian Sumpf
 * \date   2015-08-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/timer.h>
#include <hw/spec/riscv/machine_call.h>

using namespace Genode;
using namespace Kernel;


Timer_driver::Timer_driver(unsigned)
{
	/* enable timer interrupt */
	enum { STIE = 0x20 };
	asm volatile ("csrs sie, %0" : : "r"(STIE));
}

addr_t Timer_driver::stime() { return Hw::get_sys_timer(); }

void Timer::_start_one_shot(time_t const ticks)
{
	_driver.timeout = _driver.stime() + ticks;
	Hw::set_sys_timer(_driver.timeout);
}


time_t Timer::_ticks_to_us(time_t const ticks) const {
	return (ticks / Driver::TICS_PER_MS) * 1000; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * Driver::TICS_PER_MS; }


time_t Timer::_max_value() const {
	return (addr_t)~0; }


time_t Timer::_value()
{
	addr_t time = _driver.stime();
	return time < _driver.timeout ? _driver.timeout - time : 0;
}


unsigned Timer::interrupt_id() const {
	return 5; }
