/*
 * \brief   Timer implementation specific to BCM2835 System Timer
 * \author  Norman Feske
 * \author  Stefan Kalkowski
 * \author  Martin Stein
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <board.h>
#include <platform.h>
#include <kernel/timer.h>

using namespace Genode;
using namespace Kernel;
using Device = Board::Timer;


Board::Timer::Timer(unsigned)
: Mmio(Platform::mmio_to_virt(Board::SYSTEM_TIMER_MMIO_BASE)) { }


void Timer::_start_one_shot(time_t const ticks)
{
	_device.write<Device::Cs::M1>(1);
	_device.read<Device::Cs>();
	_device.write<Device::Cmp>(_device.read<Device::Clo>()
	                           + (ticks < 2 ? 2 : ticks));
}


enum { TICS_PER_US = Board::SYSTEM_TIMER_CLOCK / 1000 / 1000 };


time_t Timer::ticks_to_us(time_t const ticks) const {
	return ticks / TICS_PER_US; }


time_t Timer::us_to_ticks(time_t const us) const {
	return us * TICS_PER_US; }


time_t Timer::_max_value() const {
	return 0xffffffff; }


time_t Timer::_duration() const
{
	Device::Clo::access_t const clo = _device.read<Device::Clo>();
	Device::Cmp::access_t const cmp = _device.read<Device::Cmp>();
	Device::Cs::access_t  const irq = _device.read<Device::Cs::M1>();
	uint32_t d = (irq) ? (uint32_t)_last_timeout_duration + (clo - cmp)
	                   : clo - (cmp - _last_timeout_duration);
	return d;
}


unsigned Timer::interrupt_id() const { return Board::SYSTEM_TIMER_IRQ; }
