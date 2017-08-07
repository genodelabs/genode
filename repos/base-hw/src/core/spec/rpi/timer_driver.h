/*
 * \brief  Timer driver for core
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Kernel OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_DRIVER_H_
#define _TIMER_DRIVER_H_

/* Kernel includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Kernel { class Timer_driver; }

/**
 * Timer driver for core
 *
 * Timer channel 0 apparently doesn't work on the RPI, so we use channel 1
 */
struct Kernel::Timer_driver : Genode::Mmio
{
	enum { TICS_PER_US = Board::SYSTEM_TIMER_CLOCK / 1000 / 1000 };

	struct Cs  : Register<0x0, 32> { struct M1 : Bitfield<1, 1> { }; };
	struct Clo : Register<0x4,  32> { };
	struct Cmp : Register<0x10, 32> { };

	Timer_driver(unsigned);
};

#endif /* _TIMER_DRIVER_H_ */
