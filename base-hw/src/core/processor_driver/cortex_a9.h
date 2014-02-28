/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER__CORTEX_A9_H_
#define _PROCESSOR_DRIVER__CORTEX_A9_H_

/* core includes */
#include <processor_driver/arm_v7.h>
#include <board.h>

namespace Cortex_a9
{
	using namespace Genode;

	/**
	 * CPU driver for core
	 */
	struct Cpu : Arm_v7::Cpu
	{
		enum
		{
			/* common */
			CLK = Board::CORTEX_A9_CLOCK, /* CPU interface clock */
			PERIPH_CLK = CLK,             /* clock for CPU internal components */

			/* interrupt controller */
			PL390_DISTRIBUTOR_MMIO_BASE = Board::CORTEX_A9_PRIVATE_MEM_BASE + 0x1000,
			PL390_DISTRIBUTOR_MMIO_SIZE = 0x1000,
			PL390_CPU_MMIO_BASE = Board::CORTEX_A9_PRIVATE_MEM_BASE + 0x100,
			PL390_CPU_MMIO_SIZE = 0x100,

			/* timer */
			PRIVATE_TIMER_MMIO_BASE = Board::CORTEX_A9_PRIVATE_MEM_BASE + 0x600,
			PRIVATE_TIMER_MMIO_SIZE = 0x10,
			PRIVATE_TIMER_IRQ = 29,
			PRIVATE_TIMER_CLK = PERIPH_CLK
		};

		/**
		 * Ensure that TLB insertions get applied
		 *
		 * Nothing to do because MMU uses caches on pagetable walks
		 */
		static void tlb_insertions() { }
	};
}

#endif /* _PROCESSOR_DRIVER__CORTEX_A9_H_ */

