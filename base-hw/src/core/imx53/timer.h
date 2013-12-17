/*
 * \brief  Timer for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IMX53__TIMER_H_
#define _IMX53__TIMER_H_

/* core includes */
#include <board.h>
#include <drivers/timer/epit_base.h>

namespace Imx53
{
	/**
	 * Kernel timer
	 */
	class Timer : public Genode::Epit_base
	{
		public:

			/**
			 * Return kernel name of timer interrupt
			 */
			static unsigned interrupt_id(unsigned)
			{
				return Board::EPIT_1_IRQ;
			}

			/**
			 * Constructor
			 */
			Timer() : Epit_base(Board::EPIT_1_MMIO_BASE) { }
	};
}

namespace Kernel { class Timer : public Imx53::Timer { }; }

#endif /* _IMX53__TIMER_H_ */

