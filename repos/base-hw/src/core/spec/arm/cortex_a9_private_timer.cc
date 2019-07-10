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
#include <kernel/cpu.h>
#include <kernel/timer.h>
#include <platform.h>

using namespace Genode;
using namespace Kernel;
using Device = Board::Timer;


enum {
	TICS_PER_MS =
		Board::CORTEX_A9_PRIVATE_TIMER_CLK /
		Board::CORTEX_A9_PRIVATE_TIMER_DIV / 1000
};


Board::Timer::Timer(unsigned)
: Mmio(Platform::mmio_to_virt(Board::Cpu_mmio::PRIVATE_TIMER_MMIO_BASE))
{
	enum { PRESCALER = Board::CORTEX_A9_PRIVATE_TIMER_DIV - 1 };

	static_assert((TICS_PER_MS >= 1000) /*&&
	              (TICS_PER_US * 1000000 *
	               Board::CORTEX_A9_PRIVATE_TIMER_DIV) ==
	               Board::CORTEX_A9_PRIVATE_TIMER_CLK*/,
	              "Bad TICS_PER_US value");

	write<Load>(0xffffffff);
	Control::access_t control = 0;
	Control::Irq_enable::set(control, 1);
	Control::Prescaler::set(control, PRESCALER);
	Control::Auto_reload::set(control, 1);
	Control::Timer_enable::set(control, 1);
	write<Control>(control);
}


void Timer::_start_one_shot(time_t const ticks)
{
	/*
	 * First unset the interrupt flag,
	 * otherwise if the tick is small enough, we loose an interrupt
	 */
	_device.write<Device::Interrupt_status::Event>(1);
	_device.write<Device::Counter>(ticks);
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, TICS_PER_MS); }


unsigned Timer::interrupt_id() const {
	return Board::Cpu_mmio::PRIVATE_TIMER_IRQ; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * TICS_PER_MS; }


time_t Timer::_duration() const
{
	Device::Counter::access_t last = _last_timeout_duration;
	Device::Counter::access_t cnt  = _device.read<Device::Counter>();
	Device::Counter::access_t ret  = (_device.read<Device::Interrupt_status::Event>())
		? _max_value() - cnt + last : last - cnt;
	return ret;
}


time_t Timer::_max_value() const { return 0xfffffffe; }
