/*
 * \brief  Time source using Nova timed semaphore down
 * \author Alexander Boettcher
 * \author Martin Stein
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* NOVA includes */
#include <nova/native_thread.h>

/* local includes */
#include <time_source.h>

using namespace Genode;
using namespace Nova;


void Timer::Time_source::set_timeout(Microseconds     duration,
                                     Timeout_handler &handler)
{
	/* set new timeout parameters and wake up the blocking thread */
	Threaded_time_source::handler(handler);
	_timeout_us = duration.value;
	if (_sem) {
		if (Nova::sm_ctrl(_sem, Nova::SEMAPHORE_UP) != Nova::NOVA_OK) {
			nova_die();
		}
	}
}


Timer::Time_source::Result_of_wait_for_irq
Timer::Time_source::_wait_for_irq()
{
	/* initialize semaphore if not done yet */
	if (!_sem) {
		auto const &exc_base = Thread::native_thread().exc_pt_sel;
		request_signal_sm_cap(exc_base + Nova::PT_SEL_PAGE_FAULT,
		                      exc_base + Nova::SM_SEL_SIGNAL);

		_sem = Thread::native_thread().exc_pt_sel + SM_SEL_SIGNAL;
	}
	/* calculate absolute timeout */
	unsigned long long const deadline_timestamp {
		_timeout_us <= max_timeout().value ?
			Trace::timestamp() + _timeout_us * (_tsc_khz / TSC_FACTOR) : 0 };

	/* block until timeout fires or it gets canceled */
	switch (sm_ctrl(_sem, SEMAPHORE_DOWN, deadline_timestamp)) {

	case Nova::NOVA_TIMEOUT:
		return IRQ_TRIGGERED;

	case Nova::NOVA_OK:
		return CANCELLED;

	default:
		nova_die();
		return CANCELLED;
	}
}
