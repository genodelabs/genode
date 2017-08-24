/*
 * \brief   Timer implementation specific to Cortex A9
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

/* Genode includes */
#include <drivers/timer/util.h>

/* core includes */
#include <kernel/timer.h>
#include <platform.h>

using namespace Genode;
using namespace Kernel;


Timer_driver::Timer_driver(unsigned)
: Mmio(Platform::mmio_to_virt(Board::Cpu_mmio::PRIVATE_TIMER_MMIO_BASE))
{
	static_assert(TICS_PER_MS >= (unsigned)TIMER_MIN_TICKS_PER_MS,
	              "Bad TICS_PER_MS value");
	write<Control::Timer_enable>(0);
}


void Timer::_start_one_shot(time_t const ticks)
{
	enum { PRESCALER = Board::CORTEX_A9_PRIVATE_TIMER_DIV - 1 };

	/* reset timer */
	_driver.write<Driver::Interrupt_status::Event>(1);
	Driver::Control::access_t control = 0;
	Driver::Control::Irq_enable::set(control, 1);
	Driver::Control::Prescaler::set(control, PRESCALER);
	_driver.write<Driver::Control>(control);

	/* load timer and start decrementing */
	_driver.write<Driver::Load>(ticks);
	_driver.write<Driver::Control::Timer_enable>(1);
}


time_t Timer::_ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, Driver::TICS_PER_MS); }


unsigned Timer::interrupt_id() const {
	return Board::Cpu_mmio::PRIVATE_TIMER_IRQ; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * Driver::TICS_PER_MS; }


time_t Timer::_value() {
	return _driver.read<Driver::Counter>(); }


time_t Timer::_max_value() const {
	return (Driver::Load::access_t)~0; }
