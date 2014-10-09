/*
 * \brief   Static kernel configuration
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__CONFIGURATION_H_
#define _KERNEL__CONFIGURATION_H_

namespace Kernel
{
	enum {
		DEFAULT_STACK_SIZE   = 16 * 1024,
		MAX_PDS              = 256,
		MAX_THREADS          = 256,
		MAX_SIGNAL_RECEIVERS = 2048,
		MAX_SIGNAL_CONTEXTS  = 4096,
		MAX_VMS              = 4,
	};

	/* amount of priority bands amongst quota owners in CPU scheduling */
	constexpr unsigned cpu_priorities = 4;

	/* super period in CPU scheduling and the overall allocatable CPU time */
	constexpr unsigned cpu_quota_ms = 1000;

	/* time slice for the round-robin mode and the idle in CPU scheduling */
	constexpr unsigned cpu_fill_ms = 100;
}

#endif /* _KERNEL__CONFIGURATION_H_ */
