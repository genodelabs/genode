/*
 * \brief  Timer driver for core
 * \author Reto Buerki
 * \author Martin Stein
 * \date   2015-04-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>
#include <drivers/timer/util.h>

/* core includes */
#include <kernel/timer.h>
#include <platform.h>
#include <board.h>
#include <sinfo_instance.h>

using namespace Genode;
using namespace Kernel;


Board::Timer::Timer(unsigned) : ticks_per_ms(sinfo()->get_tsc_khz()), start(0)
{
	/* first sinfo instance, output status */
	sinfo()->log_status();

	const struct Sinfo::Resource_type *
		region = sinfo()->get_resource("timed_event", Sinfo::RES_MEMORY);
	if (!region) {
		raw("muen-timer: Unable to retrieve timed event region");
		throw Invalid_region();
	}

	event_page = (Subject_timed_event *)
		Platform::mmio_to_virt(region->data.mem.address);
	event_page->event_nr = Board::TIMER_EVENT_KERNEL;
	raw("muen-timer: Page @", Hex(region->data.mem.address), ", "
	    "frequency ", ticks_per_ms, " kHz, "
	    "event ", (unsigned)event_page->event_nr);

	region = sinfo()->get_resource("monitor_timed_event", Sinfo::RES_MEMORY);
	if (region) {
		raw("muen-timer: Found guest timed event page @", Hex(region->data.mem.address),
		    " -> enabling preemption");
		guest_event_page = (Subject_timed_event *)
			Platform::mmio_to_virt(region->data.mem.address);
		guest_event_page->event_nr = Board::TIMER_EVENT_PREEMPT;
	}
}


unsigned Timer::interrupt_id() const {
	return Board::TIMER_VECTOR_KERNEL; }


void Timer::_start_one_shot(time_t const ticks)
{
	static const time_t MIN_TICKS = 10UL;

	_device.start = _device.rdtsc();
	uint64_t t    = _device.start + ((ticks > MIN_TICKS) ? ticks : MIN_TICKS);
	_device.event_page->tsc_trigger = t;

	if (_device.guest_event_page)
		_device.guest_event_page->tsc_trigger = t;
}


time_t Timer::ticks_to_us(time_t const ticks) const {
	return timer_ticks_to_us(ticks, _device.ticks_per_ms); }


time_t Timer::us_to_ticks(time_t const us) const {
	return us * (_device.ticks_per_ms / 1000); }


time_t Timer::_max_value() const { return ~0UL; }


time_t Timer::_duration() const
{
	return _device.rdtsc() - _device.start;
}
