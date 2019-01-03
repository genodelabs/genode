/*
 * \brief  Connection to timer service and timeout scheduler
 * \author Martin Stein
 * \date   2016-11-04
 *
 * On ARM, we do not have a component-local hardware time-source. The ARM
 * performance counter has no reliable frequency as the ARM idle command
 * halts the counter. Thus, we do not do local time interpolation.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/internal/globals.h>

using namespace Genode;
using namespace Genode::Trace;

Timestamp Timer::Connection::_timestamp() { return 0ULL; }

void Timer::Connection::_update_real_time() { }

Duration Timer::Connection::curr_time()
{
	return Duration(Microseconds(elapsed_us()));
}
