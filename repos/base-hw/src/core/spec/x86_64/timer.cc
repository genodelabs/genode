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

using namespace Core;
using namespace Kernel;

Board::Timer::Timer(unsigned)
:
	Local_apic(Platform::mmio_to_virt(Hw::Cpu_memory_map::lapic_phys_base()))
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

	Platform::apply_with_boot_info([&](auto const &boot_info) {
			ticks_per_ms = boot_info.plat_info.lapic_freq_khz;
			divider = boot_info.plat_info.lapic_div;
		});
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
