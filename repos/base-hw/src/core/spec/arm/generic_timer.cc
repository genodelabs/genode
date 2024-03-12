/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <drivers/timer/util.h>
#include <kernel/timer.h>
#include <kernel/cpu.h>

using namespace Kernel;


unsigned Timer::interrupt_id() const { return Board::TIMER_IRQ; }


unsigned long Board::Timer::_freq() { return Core::Cpu::Cntfrq::read(); }


Board::Timer::Timer(unsigned) : ticks_per_ms((unsigned)(_freq() / 1000))
{
	init();
}


void Board::Timer::init()
{
	Cpu::Cntp_ctl::access_t ctl = 0;
	Cpu::Cntp_ctl::Enable::set(ctl, 1);
	Cpu::Cntp_ctl::write(ctl);
}


void Timer::_start_one_shot(time_t const ticks)
{
	Cpu::Cntp_tval::write((Cpu::Cntp_tval::access_t)ticks);
	Cpu::Cntp_ctl::access_t ctl = Cpu::Cntp_ctl::read();
	Cpu::Cntp_ctl::Istatus::set(ctl, 0);
	Cpu::Cntp_ctl::write(ctl);
}


time_t Timer::_duration() const
{
	return Cpu::Cntpct::read() - _time;
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return Genode::timer_ticks_to_us(ticks, _device.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us * _device.ticks_per_ms) / 1000; }


time_t Timer::_max_value() const {
	return _device.ticks_per_ms * 5000; }
