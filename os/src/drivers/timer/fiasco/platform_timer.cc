/*
 * \brief  Fiasco-specific sleep implementation
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/misc_math.h>
#include <base/printf.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
}

/* local includes */
#include "timer_session_component.h"

using namespace Fiasco;


static l4_timeout_s mus_to_timeout(unsigned long mus)
{
	if (mus == 0)
		return L4_IPC_TIMEOUT_0;
	else if (mus == ~0UL)
		return L4_IPC_TIMEOUT_NEVER;

	long e = Genode::log2(mus) - 7;
	unsigned long m;

	if (e < 0) e = 0;
	m = mus / (1UL << e);

	/* check corner case */
	if ((e > 31 ) || (m > 1023)) {
		PWRN("invalid timeout %ld, using max. values\n", mus);
		e = 0;
		m = 1023;
	}

	return l4_timeout_rel(m, e);
}


unsigned long Platform_timer::max_timeout()
{
	Genode::Lock::Guard lock_guard(_lock);
	return 1000*1000;
}


unsigned long Platform_timer::curr_time()
{
	Genode::Lock::Guard lock_guard(_lock);
	return _curr_time_usec;
}


void Platform_timer::_usleep(unsigned long usecs)
{
	l4_ipc_sleep(l4_timeout(L4_IPC_TIMEOUT_NEVER, mus_to_timeout(usecs)));
	_curr_time_usec += usecs;
}
