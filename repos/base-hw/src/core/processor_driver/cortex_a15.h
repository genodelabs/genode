/*
 * \brief  Processor driver for core
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
	/**
	 * Part of processor state that is not switched on every mode transition
	 */
	class Processor_lazy_state { };

	/**
	 * Processor driver for core
	 */
	struct Processor_driver : Arm_v7::Processor_driver
	{
		/**
		 * Ensure that TLB insertions get applied
		 */
		static void tlb_insertions() { }

		/**
		 * Prepare for the proceeding of a user
		 */
		static void prepare_proceeding(Processor_lazy_state *,
		                               Processor_lazy_state *) { }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Processor_lazy_state *) { return false; }
	};
}


/******************************
 ** Arm_v7::Processor_driver **
 ******************************/

void Arm_v7::Processor_driver::finish_init_phys_kernel() { }

#endif /* _PROCESSOR_DRIVER__CORTEX_A15_H_ */

