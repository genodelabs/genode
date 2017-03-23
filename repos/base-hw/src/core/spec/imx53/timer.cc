/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/timer.h>
#include <board.h>
#include <platform.h>

using namespace Genode;
using namespace Kernel;


unsigned Timer::interrupt_id() const { return Board::EPIT_1_IRQ; }


Timer_driver::Timer_driver(unsigned)
: Mmio(Platform::mmio_to_virt(Board::EPIT_1_MMIO_BASE)) { }


void Timer::_start_one_shot(time_t const ticks)
{
	/* stop timer */
	_driver.reset();

	/* configure timer for a one-shot */
	_driver.write<Driver::Cr>(Driver::Cr::prepare_one_shot());
	_driver.write<Driver::Lr>(ticks);
	_driver.write<Driver::Cmpr>(0);

	/* start timer */
	_driver.write<Driver::Cr::En>(1);
}


time_t Timer::_ticks_to_us(time_t const ticks) const {
	return (ticks / Driver::TICS_PER_MS) * 1000UL; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000UL) * Driver::TICS_PER_MS; }


time_t Timer::_max_value() const {
	return (Driver::Cnt::access_t)~0; }


time_t Timer::_value() {
	return _driver.read<Driver::Sr::Ocif>() ? 0 : _driver.read<Driver::Cnt>(); }
