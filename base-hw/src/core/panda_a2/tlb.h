/*
 * \brief  Software TLB controls specific for the PandaBoard A2
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__PANDA_A2__TLB_H_
#define _SRC__CORE__PANDA_A2__TLB_H_

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
 * TLB of core
 *
 * Must ensure that core never gets a pagefault.
 */
class Core_tlb : public Tlb
{
	public:

		Core_tlb()
		{
			using namespace Genode;

			/* map RAM */
			translate_dpm_off(Board::RAM_0_BASE, Board::RAM_0_SIZE, 0, 1);

			/* map MMIO */
			translate_dpm_off(Board::MMIO_0_BASE, Board::MMIO_0_SIZE, 1, 0);
			translate_dpm_off(Board::MMIO_1_BASE, Board::MMIO_1_SIZE, 1, 0);
		}
};

#endif /* _SRC__CORE__PANDA_A2__TLB_H_ */

