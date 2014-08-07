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

#ifndef _CPU_H_
#define _CPU_H_

/* core includes */
#include <spec/arm_v7/cpu_support.h>

namespace Genode
{
	/**
	 * Part of CPU state that is not switched on every mode transition
	 */
	class Cpu_lazy_state { };

	/**
	 * CPU driver for core
	 */
	class Cpu;
}

namespace Kernel
{
	using Genode::Cpu_lazy_state;
	using Genode::Cpu;
}

class Genode::Cpu : public Arm_v7
{
	public:

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Cpu_lazy_state *) { return false; }

		/**
		 * Return kernel name of the executing processor
		 */
		static unsigned executing_id();

		/**
		 * Return kernel name of the primary processor
		 */
		static unsigned primary_id();

		/*************
		 ** Dummies **
		 *************/

		static void tlb_insertions() { inval_branch_predicts(); }
		static void translation_added(addr_t, size_t) { }
		static void prepare_proceeding(Cpu_lazy_state *, Cpu_lazy_state *) { }
};

void Genode::Arm_v7::finish_init_phys_kernel() { }

#endif /* _CPU_H_ */
