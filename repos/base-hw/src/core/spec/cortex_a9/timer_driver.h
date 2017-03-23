/*
 * \brief  Timer implementation specific to Cortex A9
 * \author Martin stein
 * \date   2011-12-13
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_DRIVER_H_
#define _TIMER_DRIVER_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Kernel { class Timer_driver; }

/**
 * Timer driver for core
 */
struct Kernel::Timer_driver : Genode::Mmio
{
	enum {
		TICS_PER_MS =
			Board::CORTEX_A9_PRIVATE_TIMER_CLK /
			Board::CORTEX_A9_PRIVATE_TIMER_DIV / 1000
	};

	/**
	 * Load value register
	 */
	struct Load : Register<0x0, 32> { };

	/**
	 * Counter value register
	 */
	struct Counter : Register<0x4, 32> { };

	/**
	 * Timer control register
	 */
	struct Control : Register<0x8, 32>
	{
		struct Timer_enable : Bitfield<0,1> { }; /* enable counting */
		struct Irq_enable   : Bitfield<2,1> { }; /* unmask interrupt */
		struct Prescaler    : Bitfield<8,8> { };
	};

	/**
	 * Timer interrupt status register
	 */
	struct Interrupt_status : Register<0xc, 32>
	{
		struct Event : Bitfield<0,1> { }; /* if counter hit zero */
	};

	Timer_driver(unsigned);
};

#endif /* _TIMER_DRIVER_H_ */
