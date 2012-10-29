/*
 * \brief  Dummy platform-timer implementation
 * \author Norman Feske
 * \date   2009-11-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

/* local includes */
#include "timer_session_component.h"

/* Codezero includes */
#include <codezero/syscalls.h>

using namespace Codezero;


unsigned long Platform_timer::max_timeout() { return 1000; }


unsigned long Platform_timer::curr_time() const
{
	Genode::Lock::Guard lock_guard(_lock);
	return _curr_time_usec;
}


void Platform_timer::_usleep(unsigned long usecs)
{
	for (int i = 0; i < 10; i++)
		l4_thread_switch(L4_NILTHREAD);
	_curr_time_usec += 1000;
}
