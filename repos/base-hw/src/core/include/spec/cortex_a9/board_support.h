/*
 * \brief  Board driver definitions common to Cortex A9 SoCs
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__CORTEX_A9__BOARD_SUPPORT_H_
#define _SPEC__CORTEX_A9__BOARD_SUPPORT_H_

/* core includes */
#include <drivers/board_base.h>

namespace Cortex_a9
{
	/**
	 * Board driver
	 */
	class Board;
}

class Cortex_a9::Board : public Genode::Board_base
{
	public:

		enum {
			/* interrupt controller */
			IRQ_CONTROLLER_DISTR_BASE = CORTEX_A9_PRIVATE_MEM_BASE + 0x1000,
			IRQ_CONTROLLER_DISTR_SIZE = 0x1000,
			IRQ_CONTROLLER_CPU_BASE   = CORTEX_A9_PRIVATE_MEM_BASE + 0x100,
			IRQ_CONTROLLER_CPU_SIZE   = 0x100,

			/* timer */
			PRIVATE_TIMER_MMIO_BASE   = CORTEX_A9_PRIVATE_MEM_BASE + 0x600,
			PRIVATE_TIMER_MMIO_SIZE   = 0x10,
			PRIVATE_TIMER_IRQ         = 29,
		};
};

#endif /* _SPEC__CORTEX_A9__BOARD_SUPPORT_H_ */
