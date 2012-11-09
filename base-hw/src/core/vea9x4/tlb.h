/*
 * \brief  SW controls for the translation lookaside-buffer
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__VEA9X4__TLB_H_
#define _SRC__CORE__VEA9X4__TLB_H_

/* Genode includes */
#include <drivers/board.h>

/* core includes */
#include <arm/v7/section_table.h>

/**
 * Software TLB-controls
 */
class Tlb : public Arm_v7::Section_table
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
 */
class Core_tlb : public Tlb
{
	public:

		/**
		 * Constructor
		 *
		 * Must ensure that core never gets a pagefault.
		 */
		Core_tlb()
		{
			using namespace Genode;
			map_core_area(Board::RAM_0_BASE, Board::RAM_0_SIZE, 0);
			map_core_area(Board::RAM_1_BASE, Board::RAM_1_SIZE, 0);
			map_core_area(Board::RAM_2_BASE, Board::RAM_2_SIZE, 0);
			map_core_area(Board::RAM_3_BASE, Board::RAM_3_SIZE, 0);
			map_core_area(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1);
			map_core_area(Board::MMIO_1_BASE, Board::MMIO_1_SIZE, 1);
		}
};

#endif /* _SRC__CORE__VEA9X4__TLB_H_ */

