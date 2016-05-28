/*
 * \brief  Pistachio-specific sleep implementation
 * \author Julian Stecklina
 * \date   2008-03-19
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/blocking.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/ipc.h>
}

/* local includes */
#include "timer_session_component.h"


unsigned long Platform_timer::max_timeout()
{
	Genode::Lock::Guard lock_guard(_lock);
	return 1000*1000;
}


unsigned long Platform_timer::curr_time() const
{
	Genode::Lock::Guard lock_guard(_lock);
	return _curr_time_usec;
}


void Platform_timer::_usleep(unsigned long usecs)
{
	using namespace Pistachio;

	enum { MAGIC_USER_DEFINED_HANDLE = 13 };
	L4_Set_UserDefinedHandle(MAGIC_USER_DEFINED_HANDLE);

	L4_Sleep(L4_TimePeriod(usecs));
	_curr_time_usec += usecs;

	/* check if sleep was canceled */
	if (L4_UserDefinedHandle() != MAGIC_USER_DEFINED_HANDLE)
		throw Genode::Blocking_canceled();
}
