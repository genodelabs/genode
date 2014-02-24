/*
 * \brief  Transtaltion lookaside buffer
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PANDA__TLB_H_
#define _PANDA__TLB_H_

/* core includes */
#include <board.h>
#include <tlb/arm_v7.h>

namespace Genode
{
	class Tlb : public Arm_v7::Section_table { };

	/**
	 * Transtaltion lookaside buffer of core
	 */
	class Core_tlb : public Tlb
	{
		private:

			/**
			 * On Pandaboard the L2 cache needs to be disabled by a
			 * TrustZone hypervisor call
			 */
			void _disable_outer_l2_cache()
			{
				asm volatile (
					"stmfd sp!, {r0-r12} \n"
					"mov   r0, #0        \n"
					"ldr   r12, =0x102   \n"
					"dsb                 \n"
					"smc #0              \n"
					"ldmfd sp!, {r0-r12}");
			}

		public:

			/**
			 * Constructor - ensures that core never gets a pagefault
			 */
			Core_tlb()
			{
				using namespace Genode;

				/*
				 * Disable L2-cache by now, or we get into deep trouble with the MMU
				 * not using the L2 cache
				 */
				_disable_outer_l2_cache();

				map_core_area(Board::RAM_0_BASE, Board::RAM_0_SIZE, 0);
				map_core_area(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1);
				map_core_area(Board::MMIO_1_BASE, Board::MMIO_1_SIZE, 1);
			}
	};
}

#endif /* _PANDA__TLB_H_ */

