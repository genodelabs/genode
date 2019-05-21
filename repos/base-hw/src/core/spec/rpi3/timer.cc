/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <drivers/timer/util.h>
#include <kernel/timer.h>
#include <kernel/cpu.h>

using namespace Kernel;


unsigned Timer::interrupt_id() const { return Genode::Pic::TIMER_IRQ; }


unsigned long Timer_driver::_freq() { return Genode::Cpu::Cntfrq_el0::read(); }


Timer_driver::Timer_driver(unsigned) : ticks_per_ms(_freq() / 1000)
{
	Cpu::Cntp_ctl_el0::access_t ctl = 0;
	Cpu::Cntp_ctl_el0::Enable::set(ctl, 1);
	Cpu::Cntp_ctl_el0::write(ctl);
}


void Timer::_start_one_shot(time_t const ticks)
{
	_driver.last_time = Cpu::Cntpct_el0::read();
	Cpu::Cntp_tval_el0::write(ticks);
	Cpu::Cntp_ctl_el0::access_t ctl = Cpu::Cntp_ctl_el0::read();
	Cpu::Cntp_ctl_el0::Istatus::set(ctl, 0);
	Cpu::Cntp_ctl_el0::write(ctl);
}


time_t Timer::_duration() const
{
	return Cpu::Cntpct_el0::read() - _driver.last_time;
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return Genode::timer_ticks_to_us(ticks, _driver.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * _driver.ticks_per_ms; }


time_t Timer::_max_value() const {
	return _driver.ticks_per_ms * 5000; }
