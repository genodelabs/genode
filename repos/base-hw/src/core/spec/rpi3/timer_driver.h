/*
 * \brief  Timer driver for core
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Kernel OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_DRIVER_H_
#define _TIMER_DRIVER_H_

/* base-hw includes */
#include <kernel/types.h>

namespace Kernel { class Timer_driver; }


struct Kernel::Timer_driver
{
	unsigned long _freq();

	unsigned const ticks_per_ms;

	time_t last_time { 0 };

	Timer_driver(unsigned);
};

#endif /* _TIMER_DRIVER_H_ */
