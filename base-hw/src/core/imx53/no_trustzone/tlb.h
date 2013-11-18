/*
 * \brief  Translation lookaside buffer
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IMX53__TLB_H_
#define _IMX53__TLB_H_

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
				map_core_area(Board::RAM0_BASE, Board::RAM0_SIZE, 0);
				map_core_area(Board::RAM1_BASE, Board::RAM1_SIZE, 0);
				map_core_area(Board::MMIO_BASE, Board::MMIO_SIZE, 1);
			}
	};
}

#endif /* _IMX53__TLB_H_ */

