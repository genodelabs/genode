/*
 * \brief  Time source that uses the timeout syscalls of the HW kernel
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/signal.h>
#include <base/entrypoint.h>

/* local includes */
#include <time_source.h>

/* base-hw includes */
#include <kernel/interface.h>

using namespace Genode;

enum { MIN_TIMEOUT_US = 1000 };


Timer::Time_source::Time_source(Env &env)
:
	Signalled_time_source(env),
	_max_timeout_us(Kernel::timeout_max_us())
{
	if (_max_timeout_us < MIN_TIMEOUT_US) {
		error("minimum timeout greater then maximum timeout");
		throw Genode::Exception();
	}
}


void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	unsigned long duration_us = duration.value;
	if (duration_us < MIN_TIMEOUT_US) {
		duration_us = MIN_TIMEOUT_US; }

	if (duration_us > max_timeout().value) {
		duration_us = max_timeout().value; }

	_handler = &handler;
	_last_timeout_age_us = 0;
	Signal_context_capability cap = _signal_handler;
	Kernel::timeout(duration_us, (addr_t)cap.data());
}


Duration Timer::Time_source::curr_time()
{
	unsigned long const timeout_age_us = Kernel::timeout_age_us();
	if (timeout_age_us > _last_timeout_age_us) {

		/* increment time by the difference since the last update */
		_curr_time.add(Microseconds(timeout_age_us - _last_timeout_age_us));
		_last_timeout_age_us = timeout_age_us;
	}
	return _curr_time;
}
