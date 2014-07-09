/*
 * \brief  Processor driver for core
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

#ifndef _PROCESSOR_DRIVER_H_
#define _PROCESSOR_DRIVER_H_

/* core includes */
#include <spec/arm/processor_driver_support.h>
#include <assert.h>
#include <board.h>

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

class Genode::Processor_driver : public Arm
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
			struct W : Bitfield<3,1> { };  /* enable write buffer */

			struct Unused_0 : Bitfield<4,3> { }; /* shall be ones */

			struct B : Bitfield<7,1> /* Memory system endianess */
			{
				enum { LITTLE = 0 };
			};

			struct S  : Bitfield<8,1>  { }; /* enable MMU protection */
			struct R  : Bitfield<9,1>  { }; /* enable ROM protection */
			struct L4 : Bitfield<15,1> { }; /* raise T bit on LOAD-to-PC */
			struct Dt : Bitfield<16,1> { }; /* global data TCM enable */
			struct It : Bitfield<18,1> { }; /* global instruction TCM enable */
			struct U  : Bitfield<22,1> { }; /* enable unaligned data access */
			struct Xp : Bitfield<23,1> { }; /* disable subpage AP bits */

			struct Unused_1 : Bitfield<26,6> { }; /* shall not be modified */

			/**
			 * Get static base value for writes
			 */
			static access_t base_value() {
				return Unused_0::reg_mask() | Unused_1::masked(read()); }

			/**
			 * Value for the switch to virtual mode in kernel
			 */
			static access_t init_virt_kernel()
			{
				return base_value() |
				       Arm::Sctlr::init_virt_kernel() |
				       W::bits(0) |
				       B::bits(B::LITTLE) |
				       S::bits(0) |
				       R::bits(0) |
				       L4::bits(0) |
				       Dt::bits(0) |
				       It::bits(0) |
				       U::bits(0) |
				       Xp::bits(1);
			}

			/**
			 * Value for the initial kernel entry
			 */
			static access_t init_phys_kernel()
			{
				return base_value() |
				       Arm::Sctlr::init_phys_kernel() |
				       W::bits(0) |
				       B::bits(B::LITTLE) |
				       S::bits(0) |
				       R::bits(0) |
				       L4::bits(0) |
				       Dt::bits(1) |
				       It::bits(1) |
				       U::bits(0) |
				       Xp::bits(1);
			}
		};

		/**
		 * Translation table base control register 0
		 */
		struct Ttbr0 : Arm::Ttbr0
		{
			struct C : Bitfield<0,1> /* inner cachable mode */
			{
				enum { NON_CACHEABLE = 0 };
			};

			struct P : Bitfield<2,1> { }; /* memory controller ECC enabled */

			/**
			 * Value for the switch to virtual mode in kernel
			 *
			 * \param section_table  initial section table
			 */
			static access_t init_virt_kernel(addr_t const sect_table)
			{
				return Arm::Ttbr0::init_virt_kernel(sect_table) |
				       P::bits(0) |
				       C::bits(C::NON_CACHEABLE);
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
		 * \param section_table  section translation table of the initial
		 *                       address space this function switches to
		 * \param process_id     process ID of the initial address space
		 */
		static void init_virt_kernel(addr_t const section_table,
		                             unsigned const process_id)
		{
			Cidr::write(process_id);
			Dacr::write(Dacr::init_virt_kernel());
			Ttbr0::write(Ttbr0::init_virt_kernel(section_table));
			Ttbcr::write(Ttbcr::init_virt_kernel());
			Sctlr::write(Sctlr::init_virt_kernel());
		}

		/**
		 * Ensure that TLB insertions get applied
		 */
		static void tlb_insertions() { flush_tlb(); }

		static void start_secondary_processors(void * const ip)
		{
			assert(!is_smp());
		}

		/**
		 * Invalidate all predictions about the future control-flow
		 */
		static void invalidate_control_flow_predictions()
		{
			/* FIXME invalidation of branch prediction not implemented */
		}

		/**
		 * Finish all previous data transfers
		 */
		static void data_synchronization_barrier()
		{
			/* FIXME data synchronization barrier not implemented */
		}

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt()
		{
			/* FIXME cheap way of waiting is not implemented */
		}

		/**
		 * Return kernel name of the primary processor
		 */
		static unsigned primary_id() { return 0; }

		/**
		 * Return kernel name of the executing processor
		 */
		static unsigned executing_id() { return primary_id(); }


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
};


void Genode::Arm::flush_data_caches()
{
	asm volatile ("mcr p15, 0, %[rd], c7, c14, 0" :: [rd]"r"(0) : );
}


void Genode::Arm::invalidate_data_caches()
{
	asm volatile ("mcr p15, 0, %[rd], c7, c6, 0" :: [rd]"r"(0) : );
}


#endif /* _PROCESSOR_DRIVER_H_ */
