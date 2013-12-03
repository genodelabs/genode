/*
 * \brief  Timer for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXYNOS5__TIMER_H_
#define _EXYNOS5__TIMER_H_

/* core includes */
#include <board.h>
#include <timer/exynos_mct.h>

namespace Kernel
{
	/**
	 * Kernel timer
	 */
	class Timer : public Exynos_mct::Timer
	{
		public:

			enum { IRQ = Genode::Board::MCT_IRQ_L0 };

			/**
			 * Constructor
			 */
			Timer() : Exynos_mct::Timer(Genode::Board::MCT_MMIO_BASE,
			                            Genode::Board::MCT_CLOCK) { }
	};
}

#endif /* _EXYNOS5__TIMER_H_ */
