/*
 * \brief  Software TLB controls specific for the i.MX31
 * \author Norman Feske
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__IMX31__TLB_H_
#define _SRC__CORE__IMX31__TLB_H_

/* Genode includes */
#include <arm/v6/section_table.h>
#include <drivers/board.h>

/**
 * Software TLB-controls
 */
class Tlb : public Arm_v6::Section_table
{
	public:

		/**
		 * Placement new
		 */
		void * operator new (Genode::size_t, void * p) { return p; }
};

/**
 * Board specific mapping attributes
 */
struct Page_flags : Arm::Page_flags { };

typedef Arm::page_flags_t page_flags_t;

/**
 * TLB of core
 *
 * Must ensure that core never gets a pagefault.
 */
class Core_tlb : public Tlb
{
	public:

		Core_tlb()
		{
			map_core_area(Board::RAM_0_BASE, Board::RAM_0_SIZE, 0);
			map_core_area(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1);
		}
};

#endif /* _SRC__CORE__IMX31__TLB_H_ */

