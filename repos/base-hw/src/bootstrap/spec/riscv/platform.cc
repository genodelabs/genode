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
:
	early_ram_regions(Memory_region { RAM_BASE, RAM_SIZE } ),
	core_mmio()
{ }

unsigned Bootstrap::Platform::enable_mmu()
{
	using Satp = Hw::Riscv_cpu::Satp;
	using Sstatus = Hw::Riscv_cpu::Sstatus;

	/* disable supervisor interrupts */
	Sstatus::access_t sstatus = Sstatus::read();
	Sstatus::Sie::set(sstatus, 0);
	Sstatus::write(sstatus);

	/*  set page table */
	Satp::access_t satp = 0;
	Satp::Ppn::set(satp, (addr_t)core_pd->table_base >> 12);

	/* SV39 mode */
	Satp::Mode::set(satp, 8);
	Satp::write(satp);

	asm volatile ("sfence.vma" : : : "memory");

	return 0;
}
