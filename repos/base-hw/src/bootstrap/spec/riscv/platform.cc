/*
 * \brief   Platform implementations specific for RISC-V
 * \author  Stefan Kalkowski
 * \author  Sebastian Sumpf
 * \date    2016-11-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#include <platform.h>

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE } ) {}


void Bootstrap::Platform::enable_mmu()
{
	asm volatile ("csrw sptbr, %0\n" /* set asid | page table */
	              :
	              : "r" ((addr_t)core_pd->table_base >> 12)
	              :  "memory");
}
