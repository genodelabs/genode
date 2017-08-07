/*
 * \brief   Timer implementation specific to Rpi
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
#include <platform.h>
#include <kernel/timer.h>

using namespace Genode;
using namespace Kernel;


Timer_driver::Timer_driver(unsigned)
: Mmio(Platform::mmio_to_virt(Board::SYSTEM_TIMER_MMIO_BASE)) { }


void Timer::_start_one_shot(time_t const ticks)
{
	_driver.write<Driver::Cs::M1>(1);
	_driver.read<Driver::Cs>();
	_driver.write<Driver::Clo>(0);
	_driver.write<Driver::Cmp>(_driver.read<Driver::Clo>() + ticks);
}


time_t Timer::_ticks_to_us(time_t const ticks) const {
	return ticks / Driver::TICS_PER_US; }


time_t Timer::us_to_ticks(time_t const us) const {
	return us * Driver::TICS_PER_US; }


time_t Timer::_max_value() const {
	return (Driver::Clo::access_t)~0; }


time_t Timer::_value()
{
	Driver::Cmp::access_t const cmp = _driver.read<Driver::Cmp>();
	Driver::Clo::access_t const clo = _driver.read<Driver::Clo>();
	return cmp > clo ? cmp - clo : 0;
}


unsigned Timer::interrupt_id() const { return Board::SYSTEM_TIMER_IRQ; }
