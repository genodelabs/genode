/*
 * \brief  Time source using Nova timed semaphore down
 * \author Alexander Boettcher
 * \author Martin Stein
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/attached_rom_dataspace.h>

/* NOVA includes */
#include <nova/native_thread.h>

/* local includes */
#include <time_source.h>

using namespace Genode;
using namespace Nova;
using Microseconds = Genode::Time_source::Microseconds;


Timer::Time_source::Time_source(Entrypoint &ep) : Threaded_time_source(ep)
{
	/* read out the tsc frequenzy once */
	Attached_rom_dataspace _ds("hypervisor_info_page");
	Nova::Hip * const hip = _ds.local_addr<Nova::Hip>();
	_tsc_khz = hip->tsc_freq;
	start();
}


void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	Threaded_time_source::handler(handler);

	/* check whether to cancel last timeout */
	if (duration.value == 0 && _sem != ~0UL) {
		uint8_t res = Nova::sm_ctrl(_sem, Nova::SEMAPHORE_UP);
		if (res != Nova::NOVA_OK)
			nova_die();
	}
	/* remember timeout to be set during wait_for_timeout call */
	_timeout_us = duration.value;
}


void Timer::Time_source::_wait_for_irq()
{
	if (_sem == ~0UL) {
		_sem = Thread::native_thread().exc_pt_sel + SM_SEL_EC; }

	addr_t sem = _sem;

	/* calculate absolute timeout */
	Trace::Timestamp now   = Trace::timestamp();
	Trace::Timestamp us_64 = _timeout_us;

	if (_timeout_us == max_timeout().value) {

		/* tsc_absolute == 0 means blocking without timeout */
		uint8_t res = sm_ctrl(sem, SEMAPHORE_DOWN, 0);
		if (res != Nova::NOVA_OK && res != Nova::NOVA_TIMEOUT) {
			nova_die(); }

	} else {

		/* block until timeout fires or it gets canceled */
		unsigned long long tsc_absolute = now + us_64 * (_tsc_khz / TSC_FACTOR);
		uint8_t res = sm_ctrl(sem, SEMAPHORE_DOWN, tsc_absolute);
		if (res != Nova::NOVA_OK && res != Nova::NOVA_TIMEOUT) {
			nova_die(); }
	}
}
