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

/* Genode includes */
#include <drivers/timer/util.h>

/* core includes */
#include <kernel/timer.h>
#include <platform.h>

using namespace Core;
using namespace Kernel;


uint32_t Board::Timer::pit_calc_timer_freq(void)
{
	uint32_t t_start, t_end;

	/* set channel gate high and disable speaker */
	outb(PIT_CH2_GATE, (uint8_t)((inb(0x61) & ~0x02) | 0x01));

	/* set timer counter (mode 0, binary count) */
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


Board::Timer::Timer(unsigned)
:
	Mmio({(char *)Platform::mmio_to_virt(Hw::Cpu_memory_map::lapic_phys_base()), Mmio::SIZE})
{
	init();
}


void Board::Timer::init()
{
	/* enable LAPIC timer in one-shot mode */
	write<Tmr_lvt::Vector>(Board::TIMER_VECTOR_KERNEL);
	write<Tmr_lvt::Delivery>(0);
	write<Tmr_lvt::Mask>(0);
	write<Tmr_lvt::Timer_mode>(0);

	/* use very same divider after ACPI resume as used during initial boot */
	if (divider) {
		write<Divide_configuration::Divide_value>((uint8_t)divider);
		return;
	}

	/* calibrate LAPIC frequency to fullfill our requirements */
	for (Divide_configuration::access_t div = Divide_configuration::Divide_value::MAX;
	     div && ticks_per_ms < TIMER_MIN_TICKS_PER_MS; div--)
	{
		if (!div){
			raw("Failed to calibrate timer frequency");
			throw Calibration_failed();
		}
		write<Divide_configuration::Divide_value>((uint8_t)div);

		/* Calculate timer frequency */
		ticks_per_ms = pit_calc_timer_freq();
		divider      = div;
	}

	/**
	 * Disable PIT timer channel. This is necessary since BIOS sets up
	 * channel 0 to fire periodically.
	 */
	outb(Board::Timer::PIT_MODE, 0x30);
	outb(Board::Timer::PIT_CH0_DATA, 0);
	outb(Board::Timer::PIT_CH0_DATA, 0);
}


void Timer::_start_one_shot(time_t const ticks) {
	_device.write<Board::Timer::Tmr_initial>((uint32_t)ticks); }


time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, _device.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us * _device.ticks_per_ms) / 1000; }


time_t Timer::_max_value() const {
	return (Board::Timer::Tmr_initial::access_t)~0; }


time_t Timer::_duration() const {
	return _last_timeout_duration - _device.read<Board::Timer::Tmr_current>(); }


unsigned Timer::interrupt_id() const {
	return Board::TIMER_VECTOR_KERNEL; }
