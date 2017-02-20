/*
 * \brief  Time source that uses sleeping by the means of the kernel
 * \author Julian Stecklina
 * \author Martin Stein
 * \date   2008-03-19
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/blocking.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/ipc.h>
}

/* local includes */
#include <time_source.h>

using namespace Genode;
using Microseconds = Genode::Time_source::Microseconds;


Microseconds Timer::Time_source::max_timeout() const
{
	Lock::Guard lock_guard(_lock);
	return Microseconds(1000 * 1000);
}


Microseconds Timer::Time_source::curr_time() const
{
	Lock::Guard lock_guard(_lock);
	return Microseconds(_curr_time_us);
}


void Timer::Time_source::_usleep(unsigned long us)
{
	using namespace Pistachio;

	enum { MAGIC_USER_DEFINED_HANDLE = 13 };
	L4_Set_UserDefinedHandle(MAGIC_USER_DEFINED_HANDLE);

	L4_Sleep(L4_TimePeriod(us));
	_curr_time_us += us;

	/* check if sleep was canceled */
	if (L4_UserDefinedHandle() != MAGIC_USER_DEFINED_HANDLE)
		throw Blocking_canceled();
}
