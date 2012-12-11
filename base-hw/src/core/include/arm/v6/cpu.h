/*
 * \brief  Simple driver for the ARMv6 CPU core
 * \author Norman Feske
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM_V6__CPU_H_
#define _INCLUDE__ARM_V6__CPU_H_

/* Genode includes */
#include <drivers/board.h>
#include <base/printf.h>

/* core includes */
#include <arm/cpu.h>

namespace Arm_v6
{
	using namespace Genode;

	/**
	 * ARMv6 core
	 */
	struct Cpu : Arm::Cpu
	{
		/**
		 * Cache type register
		 */
		struct Ctr : Arm::Cpu::Ctr
		{
			struct P : Bitfield<23, 1> { }; /* page mapping restriction on */
		};

		/**
		 * System control register
		 */
		struct Sctlr : Arm::Cpu::Sctlr
		{
			struct W : Bitfield<3,1> { };  /* enable write buffer */

			struct Unused_0 : Bitfield<4,3> { }; /* shall be ones */

			struct B : Bitfield<7,1> /* Memory system endianess */
			{
				enum { LITTLE = 0 };
			};

			struct S  : Bitfield<8,1>  { }; /* enbale MMU protection */
			struct R  : Bitfield<9,1>  { }; /* enbale ROM protection */
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
				       Arm::Cpu::Sctlr::init_virt_kernel() |
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
				       Arm::Cpu::Sctlr::init_phys_kernel() |
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
		struct Ttbr0 : Arm::Cpu::Ttbr0
		{
			struct P : Bitfield<2,1> { }; /* memory controller ECC enabled */

			/**
			 * Value for the switch to virtual mode in kernel
			 *
			 * \param section_table  initial section table
			 */
			static access_t init_virt_kernel(addr_t const sect_table)
			{
				return Arm::Cpu::Ttbr0::init_virt_kernel(sect_table) |
				       P::bits(0);
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
			if (restricted_page_mappings()) {
				PDBG("Insufficient driver for page tables");
				while (1) ;
			}
		}

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param section_table  section translation table of the initial
		 *                       address space this function switches to
		 * \param process_id     process ID of the initial address space
		 */
		static void init_virt_kernel(addr_t const section_table,
		                             unsigned long const process_id)
		{
			Cidr::write(process_id);
			Dacr::write(Dacr::init_virt_kernel());
			Ttbr0::write(Ttbr0::init_virt_kernel(section_table));
			Ttbcr::write(Ttbcr::init_virt_kernel());
			Sctlr::write(Sctlr::init_virt_kernel());
		}
	};
}


void Arm::Cpu::flush_data_caches() {
	asm volatile ("mcr p15, 0, %[rd], c7, c14, 0" :: [rd]"r"(0) : ); }


#endif /* _INCLUDE__ARM_V6__CPU_H_ */

