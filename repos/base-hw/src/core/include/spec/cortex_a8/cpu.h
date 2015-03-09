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

namespace Kernel { using Genode::Cpu_lazy_state; }

class Genode::Cpu : public Arm_v7
{
	public:

		/**
		 * Ensure that TLB insertions get applied
		 */
		static void tlb_insertions() { flush_tlb(); }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Cpu_lazy_state *) { return false; }

		/**
		 * Post processing after a translation was added to a translation table
		 *
		 * \param addr  virtual address of the translation
		 * \param size  size of the translation
		 */
		static void translation_added(addr_t const addr, size_t const size)
		{
			/*
			 * The Cortex-A8 CPU can't use the L1 cache on page-table
			 * walks. Therefore, as the page-tables lie in write-back cacheable
			 * memory we've to clean the corresponding cache-lines even when a
			 * page table entry is added. We only do this as core as the kernel
			 * adds translations solely before MMU and caches are enabled.
			 */
			if (is_user()) Kernel::update_data_region(addr, size);
			else flush_data_caches();
		}

		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id();

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id();

		/*************
		 ** Dummies **
		 *************/

		static void prepare_proceeding(Cpu_lazy_state *, Cpu_lazy_state *) { }
};

void Genode::Arm_v7::finish_init_phys_kernel() { }

#endif /* _CPU_H_ */
