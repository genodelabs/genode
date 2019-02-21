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
	Kernel::timeout_t duration_us = duration.value;
	if (duration_us < MIN_TIMEOUT_US) {
		duration_us = MIN_TIMEOUT_US; }

	if (duration_us > _max_timeout_us) {
		duration_us = _max_timeout_us; }

	_handler = &handler;
	Signal_context_capability cap = _signal_handler;
	Kernel::timeout(duration_us, (addr_t)cap.data());
}


Duration Timer::Time_source::curr_time()
{
	/*
	 * FIXME: the `Microseconds` constructor takes a machine-word as value
	 * thereby limiting the possible value to something ~1.19 hours.
	 * must be changed when the timeout framework internally does not use
	 * machine-word wide microseconds values anymore.
	 */
	return Duration(Microseconds(Kernel::time()));
}
