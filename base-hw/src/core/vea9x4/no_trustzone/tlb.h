/*
 * \brief  Translation lookaside buffer
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VEA9X4__TLB_H_
#define _VEA9X4__TLB_H_

/* core includes */
#include <board.h>
#include <tlb/arm_v7.h>

namespace Genode
{
	class Tlb : public Arm_v7::Section_table { };

	/**
	 * Translation lookaside buffer of core
	 */
	class Core_tlb : public Tlb
	{
		public:

			/**
			 * Constructor - ensures that core never gets a pagefault
			 */
			Core_tlb()
			{
				map_core_area(Board::RAM_0_BASE, Board::RAM_0_SIZE, 0);
				map_core_area(Board::RAM_1_BASE, Board::RAM_1_SIZE, 0);
				map_core_area(Board::RAM_2_BASE, Board::RAM_2_SIZE, 0);
				map_core_area(Board::RAM_3_BASE, Board::RAM_3_SIZE, 0);
				map_core_area(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1);
				map_core_area(Board::MMIO_1_BASE, Board::MMIO_1_SIZE, 1);
			}
	};
}

#endif /* _VEA9X4__TLB_H_ */

