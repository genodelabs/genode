/*
 * \brief   Static kernel configuration
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CONFIGURATION_H_
#define _CORE__KERNEL__CONFIGURATION_H_

#include <kernel/interface.h>

namespace Kernel
{
	enum {
		DEFAULT_STACK_SIZE = 16 * 1024,
		DEFAULT_TRANSLATION_TABLE_MAX = 128,
	};

	/* amount of priority bands amongst quota owners in CPU scheduling */
	constexpr unsigned cpu_priorities = 4;

	/* super period in CPU scheduling and the overall allocatable CPU time */
	constexpr time_t cpu_quota_us = 1000000;

	/* time slice for the round-robin mode and the idle in CPU scheduling */
	constexpr time_t cpu_fill_us = 10000;
}

#endif /* _CORE__KERNEL__CONFIGURATION_H_ */
