/*
 * \brief  Translation lookaside buffer
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXYNOS5__TLB_H_
#define _EXYNOS5__TLB_H_

/* core includes */
#include <board.h>
#include <tlb/arm_v7.h>

namespace Genode
{
	struct Page_flags : Arm::Page_flags { };

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
				using namespace Genode;
				map_core_area(Board::RAM_0_BASE, Board::RAM_0_SIZE, 0);
				map_core_area(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1);
			}
	};
}

#endif /* _EXYNOS5__TLB_H_ */

