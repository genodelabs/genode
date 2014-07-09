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

#ifndef _SPEC__CORTEX_A15__PROCESSOR_DRIVER_SUPPORT_H_
#define _SPEC__CORTEX_A15__PROCESSOR_DRIVER_SUPPORT_H_

/* core includes */
#include <spec/arm_v7/processor_driver_support.h>

namespace Genode
{
	/**
	 * Part of processor state that is not switched on every mode transition
	 */
	class Processor_lazy_state { };

	/**
	 * Processor driver for core
	 */
	class Cortex_a15;
}

class Genode::Cortex_a15 : public Arm_v7
{
	public:

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Processor_lazy_state *) { return false; }

		/*************
		 ** Dummies **
		 *************/

		static void tlb_insertions() { }
		static void translation_added(addr_t, size_t) { }
		static void prepare_proceeding(Processor_lazy_state *,
		                               Processor_lazy_state *) { }
};


void Genode::Arm_v7::finish_init_phys_kernel() { }


#endif /* _SPEC__CORTEX_A15__PROCESSOR_DRIVER_SUPPORT_H_ */
