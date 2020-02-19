/*
 * \brief  Time source that uses sleeping by the means of the kernel
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <time_source.h>

using namespace Genode;


void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	Mutex::Guard mutex_guard(_mutex);
	Threaded_time_source::handler(handler);
	_next_timeout_us = duration.value;
}


void Timer::Time_source::_wait_for_irq()
{
	enum { SLEEP_GRANULARITY_US = 1000 };
	uint64_t last_time_us = curr_time().trunc_to_plain_us().value;
	_mutex.acquire();
	while (_next_timeout_us > 0) {
		_mutex.release();

		try { _usleep(SLEEP_GRANULARITY_US); }
		catch (Blocking_canceled) { }

		uint64_t curr_time_us = curr_time().trunc_to_plain_us().value;
		uint64_t sleep_duration_us = curr_time_us - last_time_us;
		last_time_us = curr_time_us;

		_mutex.acquire();
		if (_next_timeout_us >= sleep_duration_us)
			_next_timeout_us -= sleep_duration_us;
		else
			break;
	}
	_mutex.release();
}
