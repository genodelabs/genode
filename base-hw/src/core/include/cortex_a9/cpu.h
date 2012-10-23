/*
 * \brief  Simple driver for the Cortex A9
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CORTEX_A9__CPU_H_
#define _INCLUDE__CORTEX_A9__CPU_H_

/* Genode includes */
#include <drivers/board.h>

/* core includes */
#include <arm/v7/cpu.h>

namespace Cortex_a9
{
	using namespace Genode;

	/**
	 * Cortex A9 CPU
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
	};
}

#endif /* _INCLUDE__CORTEX_A9__CPU_H_ */

