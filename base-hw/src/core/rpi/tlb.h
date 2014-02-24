/*
 * \brief  Translation lookaside buffer
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

#ifndef _RPI__TLB_H_
#define _RPI__TLB_H_

/* core includes */
#include <tlb/arm_v6.h>
#include <board.h>

namespace Genode
{
	class Tlb : public Arm_v6::Section_table { };

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
				map_core_area(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1);
			}
	};
}

#endif /* _RPI__TLB_H_ */

