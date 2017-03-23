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

/* core includes */
#include <kernel/timer.h>
#include <platform.h>

using namespace Genode;
using namespace Kernel;


Timer_driver::Timer_driver(unsigned)
: Mmio(Platform::mmio_to_virt(Board::Cpu_mmio::PRIVATE_TIMER_MMIO_BASE))
{
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


time_t Timer::_ticks_to_us(time_t const ticks) const
{
	/*
	 * If we would do the translation with one division and
	 * multiplication over the whole argument, we would loose
	 * microseconds granularity although the timer frequency would
	 * allow for such granularity. Thus, we treat the most significant
	 * half and the least significant half of the argument separate.
	 * Each half is shifted to the best bit position for the
	 * translation, then translated, and then shifted back.
	 */
	static_assert(Driver::TICS_PER_MS >= 1000, "Bad TICS_PER_MS value");
	enum {
		HALF_WIDTH = (sizeof(time_t) << 2),
		MSB_MASK  = ~0UL << HALF_WIDTH,
		LSB_MASK  = ~0UL >> HALF_WIDTH,
		MSB_RSHIFT = 10,
		LSB_LSHIFT = HALF_WIDTH - MSB_RSHIFT,
	};
	time_t const msb = (((ticks >> MSB_RSHIFT)
	                     * 1000) / Driver::TICS_PER_MS) << MSB_RSHIFT;
	time_t const lsb = ((((ticks & LSB_MASK) << LSB_LSHIFT)
	                     * 1000) / Driver::TICS_PER_MS) >> LSB_LSHIFT;
	return (msb & MSB_MASK) | lsb;
}


unsigned Timer::interrupt_id() const {
	return Board::Cpu_mmio::PRIVATE_TIMER_IRQ; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * Driver::TICS_PER_MS; }


time_t Timer::_value() {
	return _driver.read<Driver::Counter>(); }


time_t Timer::_max_value() const {
	return (Driver::Load::access_t)~0; }
