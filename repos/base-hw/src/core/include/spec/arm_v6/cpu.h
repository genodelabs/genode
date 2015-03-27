/*
 * \brief  CPU driver for core
 * \author Norman Feske
 * \author Martin stein
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_H_
#define _CPU_H_

/* core includes */
#include <spec/arm/cpu_support.h>
#include <assert.h>
#include <board.h>

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

namespace Kernel {
	using Genode::Cpu_lazy_state;

	class Pd;
}

class Genode::Cpu : public Arm
{
	public:

		/**
		 * Cache type register
		 */
		struct Ctr : Arm::Ctr
		{
			struct P : Bitfield<23, 1> { }; /* page mapping restriction on */
		};

		/**
		 * System control register
		 */
		struct Sctlr : Arm::Sctlr
		{
			struct W  : Bitfield<3,1>  { }; /* enable write buffer */
			struct Dt : Bitfield<16,1> { }; /* global data TCM enable */
			struct It : Bitfield<18,1> { }; /* global instruction TCM enable */
			struct U  : Bitfield<22,1> { }; /* enable unaligned data access */
			struct Xp : Bitfield<23,1> { }; /* disable subpage AP bits */
			struct Unnamed_0 : Bitfield<4,3>  { }; /* shall be ones */
			struct Unnamed_1 : Bitfield<26,6> { }; /* shall not be modified */

			/**
			 * Initialization that is common
			 */
			static void init_common(access_t & v)
			{
				Arm::Sctlr::init_common(v);
				W::set(v, 1);
				Dt::set(v, 1);
				It::set(v, 1);
				U::set(v, 1);
				Xp::set(v, 1);
				Unnamed_0::set(v, ~0);
				Unnamed_1::set(v, Unnamed_1::masked(read()));
			}

			/**
			 * Initialization for physical kernel stage
			 */
			static access_t init_virt_kernel()
			{
				access_t v = 0;
				init_common(v);
				Arm::Sctlr::init_virt_kernel(v);
				return v;
			}

			/**
			 * Initialization for physical kernel stage
			 */
			static access_t init_phys_kernel()
			{
				access_t v = 0;
				init_common(v);
				return v;
			}
		};

		/**
		 * If page descriptor bits [13:12] are restricted
		 */
		static bool restricted_page_mappings() {
			return Ctr::P::get(Ctr::read()); }

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		static void init_phys_kernel()
		{
			Board::prepare_kernel();
			Sctlr::write(Sctlr::init_phys_kernel());
			flush_tlb();

			/* check for mapping restrictions */
			assert(!restricted_page_mappings());
		}

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param pd  kernel's pd object
		 */
		static void init_virt_kernel(Kernel::Pd* pd);

		/**
		 * Ensure that TLB insertions get applied
		 */
		static void tlb_insertions() { flush_tlb(); }

		static void start_secondary_cpus(void *) { assert(!Board::is_smp()); }

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
		static void wait_for_interrupt() { /* FIXME */ }
		static void data_synchronization_barrier() { /* FIXME */ }
		static void invalidate_control_flow_predictions() { /* FIXME */ }
};

#endif /* _CPU_H_ */
