/*
 * \brief  Timer driver for core
 * \author Reto Buerki
 * \date   2015-04-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <timer.h>
#include <platform.h>

Genode::Timer::Timer() : _tics_per_ms(sinfo()->get_tsc_khz())
{

	/* first sinfo instance, output status */
	sinfo()->log_status();

	struct Sinfo::Memregion_info region;
	if (!sinfo()->get_memregion_info("timed_event", &region)) {
		error("muen-timer: Unable to retrieve timed event region");
		throw Invalid_region();
	}

	_event_page = (Subject_timed_event *)Platform::mmio_to_virt(region.address);
	_event_page->event_nr = Board::TIMER_EVENT_KERNEL;
	log("muen-timer: Page @", Hex(region.address), ", "
	    "frequency ", _tics_per_ms, " kHz, "
	    "event ", (unsigned)_event_page->event_nr);

	if (sinfo()->get_memregion_info("monitor_timed_event", &region)) {
		log("muen-timer: Found guest timed event page @", Hex(region.address),
		    " -> enabling preemption");
		_guest_event_page = (Subject_timed_event *)Platform::mmio_to_virt(region.address);
		_guest_event_page->event_nr = Board::TIMER_EVENT_PREEMPT;
	}
}

