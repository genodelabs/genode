/*
 * \brief  Timer driver for core
 * \author Sebastian Sumpf
 * \date   2015-08-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* Genode includes */
#include <base/stdint.h>

namespace Kernel { class Timer_driver; }


/**
 * Timer driver for core
 */
struct Kernel::Timer_driver
{
	enum {
		SPIKE_TIMER_HZ = 1000000,
		TICS_PER_MS    = SPIKE_TIMER_HZ / 1000,
	};

	addr_t timeout = 0;

	addr_t stime();

	Timer_driver(unsigned);
};

#endif /* _TIMER_H_ */
