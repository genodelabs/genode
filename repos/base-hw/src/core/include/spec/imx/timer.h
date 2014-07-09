/*
 * \brief  Timer driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* Genode includes */
#include <drivers/timer/epit_base.h>

/* core includes */
#include <board.h>

namespace Genode
{
	/**
	 * Timer driver for core
	 */
	class Timer;
}

class Genode::Timer : public Epit_base
{
	public:

		/**
		 * Return kernel name of timer interrupt
		 */
		static unsigned interrupt_id(unsigned) { return Board::EPIT_1_IRQ; }

		/**
		 * Constructor
		 */
		Timer() : Epit_base(Board::EPIT_1_MMIO_BASE) { }
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
