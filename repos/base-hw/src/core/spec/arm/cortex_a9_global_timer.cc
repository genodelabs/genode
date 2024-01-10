/*
 * \brief   Global timer implementation specific to Cortex A9
 * \author  Johannes Schlatow
 * \date    2023-01-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <drivers/timer/util.h>

/* core includes */
#include <kernel/cpu.h>
#include <kernel/timer.h>
#include <platform.h>

using namespace Core;
using namespace Kernel;

using Device    = Board::Timer;
using counter_t = Board::Timer::Counter::access_t;


enum {
	TICS_PER_MS =
		Board::CORTEX_A9_GLOBAL_TIMER_CLK /
		Board::CORTEX_A9_GLOBAL_TIMER_DIV / 1000,
};


Board::Timer::Timer(unsigned cpu_id)
:
	Mmio({(char *)Platform::mmio_to_virt(Board::Cpu_mmio::GLOBAL_TIMER_MMIO_BASE), Mmio::SIZE})
{
	enum { PRESCALER = Board::CORTEX_A9_GLOBAL_TIMER_DIV - 1 };

	static_assert((TICS_PER_MS >= 1000),
	              "Bad TICS_PER_US value");

	/* primary CPU sets initial timer value */
	if (cpu_id == 0) {
		write<Control::Timer_enable>(0);
		write<Counter>(0, 0);
		write<Counter>(0, 1);
	}

	Control::access_t control = 0;
	Control::Irq_enable::set(control, 1);
	Control::Prescaler::set(control, PRESCALER);
	Control::Timer_enable::set(control, 1);
	write<Control>(control);
}


time_t Board::Timer::current_ticks() const
{
	uint32_t upper     = read<Counter>(1);
	uint32_t lower     = read<Counter>(0);
	uint32_t upper_new = read<Counter>(1);

	while (upper != upper_new) {
		upper     = upper_new;
		lower     = read<Counter>(0);
		upper_new = read<Counter>(1);
	}

	return (time_t)upper << 32 | (time_t)lower;
}


void Timer::_start_one_shot(time_t const ticks)
{
	/*
	 * First unset the interrupt flag,
	 * otherwise if the tick is small enough, we loose an interrupt
	 */
	_device.write<Device::Interrupt_status::Event>(1);

	/* Disable comparator before setting a new value */
	_device.write<Device::Control::Comp_enable>(0);

	time_t end_ticks = _device.current_ticks() + ticks;
	_device.write<Device::Comparator>((uint32_t)end_ticks, 0);
	_device.write<Device::Comparator>((uint32_t)(end_ticks >> 32) , 1);

	/* Enable comparator before setting a new value */
	_device.write<Device::Control::Comp_enable>(1);
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, TICS_PER_MS); }


unsigned Timer::interrupt_id() const {
	return Board::Cpu_mmio::GLOBAL_TIMER_IRQ; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * TICS_PER_MS; }


time_t Timer::_duration() const {
	return _device.current_ticks() - _time; }


time_t Timer::_max_value() const {
	return TICS_PER_MS * 5000; }
