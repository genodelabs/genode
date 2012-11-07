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
 * TLB of core
 *
 * Must ensure that core never gets a pagefault.
 */
class Core_tlb : public Tlb
{
	public:

		Core_tlb()
		{
			/* map RAM */
			translate_dpm_off(Board::RAM_0_BASE, Board::RAM_0_SIZE, 0, 1);

			/* map MMIO */
			translate_dpm_off(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1, 0);
		}
};

#endif /* _SRC__CORE__IMX31__TLB_H_ */

