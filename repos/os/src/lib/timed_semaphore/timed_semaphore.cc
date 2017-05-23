/*
 * \brief  Semaphore implementation with timeout facility.
 * \author Stefan Kalkowski
 * \date   2010-03-05
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <os/timed_semaphore.h>


Genode::Env *Genode::Timeout_thread::_env = nullptr;


void Genode::Timeout_thread::entry()
{
	while (true) {
		Signal s = _receiver.wait_for_signal();

		/* handle timouts of this point in time */
		Genode::Alarm_scheduler::handle(_timer.elapsed_ms());
	}
}


Genode::Timeout_thread *Genode::Timeout_thread::alarm_timer()
{
	static Timeout_thread _alarm_timer;
	return &_alarm_timer;
}
