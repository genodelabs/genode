/*
 * \brief  Timer driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <timer.h>
#include <platform.h>

using namespace Genode;
using Kernel::time_t;

uint32_t Timer::_pit_calc_timer_freq(void)
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


Timer::Timer() : Mmio(Platform::mmio_to_virt(Board::MMIO_LAPIC_BASE))
{
	write<Divide_configuration::Divide_value>(
		Divide_configuration::Divide_value::MAX);

	/* Enable LAPIC timer in one-shot mode */
	write<Tmr_lvt::Vector>(Board::TIMER_VECTOR_KERNEL);
	write<Tmr_lvt::Delivery>(0);
	write<Tmr_lvt::Mask>(0);
	write<Tmr_lvt::Timer_mode>(0);

	/* Calculate timer frequency */
	_tics_per_ms = _pit_calc_timer_freq();
}


void Timer::disable_pit()
{
	outb(PIT_MODE, 0x30);
	outb(PIT_CH0_DATA, 0);
	outb(PIT_CH0_DATA, 0);
}


void Timer::start_one_shot(time_t const tics, unsigned const) {
	write<Tmr_initial>(tics); }


time_t Timer::tics_to_us(time_t const tics) const {
	return (tics / _tics_per_ms) * 1000; }


time_t Timer::us_to_tics(time_t const us) const {
	return (us / 1000) * _tics_per_ms; }


time_t Timer::max_value() { return (Tmr_initial::access_t)~0; }


time_t Timer::value(unsigned const) { return read<Tmr_current>(); }
