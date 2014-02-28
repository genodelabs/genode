/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER__CORTEX_A15_H_
#define _PROCESSOR_DRIVER__CORTEX_A15_H_

/* Genode includes */
#include <util/register.h>
#include <base/printf.h>

/* core includes */
#include <processor_driver/arm_v7.h>
#include <board.h>

namespace Cortex_a15
{
	using namespace Genode;

	/**
	 * CPU driver for core
	 */
	struct Cpu : Arm_v7::Cpu
	{
		/**
		 * Ensure that TLB insertions get applied
		 *
		 * Nothing to do because MMU uses caches on pagetable walks
		 */
		static void tlb_insertions() { }
	};
}

#endif /* _PROCESSOR_DRIVER__CORTEX_A15_H_ */

