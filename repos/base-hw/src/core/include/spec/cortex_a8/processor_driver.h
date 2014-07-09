/*
 * \brief  Processor driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER_H_
#define _PROCESSOR_DRIVER_H_

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
	class Processor_driver;
}

class Genode::Processor_driver : public Arm_v7
{
	public:

		/**
		 * Ensure that TLB insertions get applied
		 */
		static void tlb_insertions() { flush_tlb(); }

		/**
		 * Prepare for the proceeding of a user
		 */
		static void prepare_proceeding(Processor_lazy_state *,
		                               Processor_lazy_state *) { }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Processor_lazy_state *) { return false; }

		/**
		 * Post processing after a translation was added to a translation table
		 *
		 * \param addr  virtual address of the translation
		 * \param size  size of the translation
		 */
		static void translation_added(addr_t const addr, size_t const size)
		{
			/*
			 * The Cortex A8 processor can't use the L1 cache on page-table
			 * walks. Therefore, as the page-tables lie in write-back cacheable
			 * memory we've to clean the corresponding cache-lines even when a
			 * page table entry is added. We only do this as core as the kernel
			 * adds translations solely before MMU and caches are enabled.
			 */
			if (is_user()) Kernel::update_data_region(addr, size);
		}

		/**
		 * Return kernel name of the primary processor
		 */
		static unsigned primary_id() { return 0; }

		/**
		 * Return kernel name of the executing processor
		 */
		static unsigned executing_id() { return primary_id(); }
};


void Genode::Arm_v7::finish_init_phys_kernel() { }


#endif /* _PROCESSOR_DRIVER_H_ */
