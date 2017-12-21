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
#include <hw/spec/riscv/cpu.h>

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE } ), core_mmio() {}


unsigned Bootstrap::Platform::enable_mmu()
{
	using Sptbr = Hw::Riscv_cpu::Sptbr;
	Sptbr::write(Sptbr::Ppn::masked((addr_t)core_pd->table_base >> 12));

	return 0;
}
