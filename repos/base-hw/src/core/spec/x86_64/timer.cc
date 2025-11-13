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

Board::Timer::Timer(Hw::X86_64_cpu::Id)
:
	Apic(Core::Platform::mmio_to_virt(Hw::Cpu_memory_map::lapic_phys_base()))
{
	Core::Platform::apply_with_boot_info([&](auto const &boot_info) {
		tsc_ticks_per_ms = boot_info.plat_info.tsc_freq_khz;
		ticks_per_ms     = boot_info.plat_info.apic_freq_khz;
		divider          = boot_info.plat_info.apic_div;
	});

	timer_init(Board::TIMER_VECTOR_KERNEL, (uint8_t)divider);
}


void Timer::_start_one_shot(time_t const ticks) {
	_device.timer_reset_ticks((uint32_t)ticks); }


time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, _device.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return (us * _device.ticks_per_ms) / 1000; }


time_t Timer::_max_value() const {
	return (Board::Timer::Tmr_initial::access_t)~0; }


time_t Timer::_duration() const {
	return us_to_ticks(timer_ticks_to_us(Genode::Trace::timestamp(), _device.tsc_ticks_per_ms)) - _time; }


unsigned Timer::interrupt_id() const {
	return Board::TIMER_VECTOR_KERNEL; }
