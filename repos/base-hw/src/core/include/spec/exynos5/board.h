/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__EXYNOS5__BOARD_H_
#define _CORE__INCLUDE__SPEC__EXYNOS5__BOARD_H_

/* core includes */
#include <spec/cortex_a15/board_support.h>

namespace Genode
{
	class Board : public Cortex_a15::Board_base
	{
		public:

			static void init() { }

			/**
			 * Tell secondary CPUs to start execution from instr. pointer 'ip'
			 */
			static void wake_up_all_cpus(void * const ip)
			{
				*(void * volatile *)IRAM_BASE = ip;
				asm volatile("dsb\n"
							 "sev\n");
			}

			static constexpr bool SMP = true;
	};
}

#endif /* _CORE__INCLUDE__SPEC__EXYNOS5__BOARD_H_ */
