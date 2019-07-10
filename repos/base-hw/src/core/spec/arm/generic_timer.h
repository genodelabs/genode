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

#ifndef _SRC__CORE__SPEC__ARM__GENERIC_TIMER_H_
#define _SRC__CORE__SPEC__ARM__GENERIC_TIMER_H_

/* base-hw includes */
#include <kernel/types.h>

namespace Board { class Timer; }

struct Board::Timer
{
	unsigned long _freq();

	unsigned const ticks_per_ms;

	Kernel::time_t last_time { 0 };

	Timer(unsigned);
};

#endif /* _SRC__CORE__SPEC__ARM__GENERIC_TIMER_H_ */
