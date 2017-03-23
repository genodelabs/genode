/*
 * \brief  Timer driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \author Martin Stein
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <hw/spec/x86_64/x86_64.h>

/* core includes */
#include <kernel/timer.h>
#include <platform.h>

using namespace Genode;
using namespace Kernel;


uint32_t Timer_driver::pit_calc_timer_freq(void)
{
	uint32_t t_start, t_end;

	/* Set channel gate high and disable speaker */
	outb(PIT_CH2_GATE, (inb(0x61) & ~0x02) | 0x01);

	/* Set timer counter (mode 0, binary count) */
	outb(PIT_MODE, 0xb0);
	outb(PIT_CH2_DATA, PIT_SLEEP_TICS & 0xff);
	outb(PIT_CH2_DATA, PIT_SLEEP_TICS >> 8);

	write<Tmr_initial>(~0U);

	t_start = read<Tmr_current>();
	while ((inb(PIT_CH2_GATE) & 0x20) == 0)
	{
		asm volatile("pause" : : : "memory");
	}
	t_end = read<Tmr_current>();

	write<Tmr_initial>(0);

	return (t_start - t_end) / PIT_SLEEP_MS;
}


Timer_driver::Timer_driver(unsigned)
: Mmio(Platform::mmio_to_virt(Hw::Cpu_memory_map::MMIO_LAPIC_BASE))
{
	write<Divide_configuration::Divide_value>(
		Divide_configuration::Divide_value::MAX);

	/* Enable LAPIC timer in one-shot mode */
	write<Tmr_lvt::Vector>(Board::TIMER_VECTOR_KERNEL);
	write<Tmr_lvt::Delivery>(0);
	write<Tmr_lvt::Mask>(0);
	write<Tmr_lvt::Timer_mode>(0);

	/* Calculate timer frequency */
	ticks_per_ms = pit_calc_timer_freq();
}


void Timer::init_cpu_local()
{
	/**
	 * Disable PIT timer channel. This is necessary since BIOS sets up
	 * channel 0 to fire periodically.
	 */
	outb(Driver::PIT_MODE, 0x30);
	outb(Driver::PIT_CH0_DATA, 0);
	outb(Driver::PIT_CH0_DATA, 0);
}


void Timer::_start_one_shot(time_t const ticks) {
	_driver.write<Driver::Tmr_initial>(ticks); }


time_t Timer::_ticks_to_us(time_t const ticks) const {
	return (ticks / _driver.ticks_per_ms) * 1000; }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us / 1000) * _driver.ticks_per_ms; }


time_t Timer::_max_value() const {
	return (Driver::Tmr_initial::access_t)~0; }


time_t Timer::_value() {
	return _driver.read<Driver::Tmr_current>(); }


unsigned Timer::interrupt_id() const {
	return Board::TIMER_VECTOR_KERNEL; }
