/*
 * \brief  Semaphore implementation with timeout facility.
 * \author Stefan Kalkowski
 * \date   2010-03-05
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/timed_semaphore.h>


void Genode::Timeout_thread::entry()
{
	while (true) {
		_timer.msleep(GRANULARITY_MSECS);
		_jiffies += GRANULARITY_MSECS;

		/* handle timouts of this point in time */
		Genode::Alarm_scheduler::handle(_jiffies);
	}
}


Genode::Timeout_thread *Genode::Timeout_thread::alarm_timer()
{
	static Timeout_thread _alarm_timer;
	return &_alarm_timer;
}
