/*
 * \brief  CPU driver for RISC-V
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2017-10-06
 */

/*
 * Copyright (C) 2017-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw internal includes */
#include <hw/assert.h>

/* base-hw Core includes */
#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>

using Mmu_context = Genode::Cpu::Mmu_context;


Genode::Cpu::Context::Context(bool)
{
	/*
	 * initialize cpu_exception with something that gets ignored in
	 * Thread::exception
	 */
	cpu_exception = IRQ_FLAG;
}


Mmu_context::
Mmu_context(addr_t                             page_table_base,
            Board::Address_space_id_allocator &addr_space_id_alloc)
:
	_addr_space_id_alloc(addr_space_id_alloc)
{
	Satp::Asid::set(satp, (Genode::uint8_t)_addr_space_id_alloc.alloc());
	Satp::Ppn::set(satp, page_table_base >> 12);
	Satp::Mode::set(satp, 8);
}


Mmu_context::~Mmu_context()
{
	unsigned asid = (uint16_t)Satp::Asid::get(satp); /* ASID is 16 bit */
	Cpu::invalidate_tlb_by_pid(asid);
	_addr_space_id_alloc.free(asid);
}


bool Genode::Cpu::active(Mmu_context & context)
{
	return Satp::read() == context.satp;
}


void Genode::Cpu::switch_to(Mmu_context & context)
{
	Satp::write(context.satp);
	sfence();
}


void Genode::Cpu::mmu_fault(Context &, Kernel::Thread_fault & f)
{
	f.addr = Genode::Cpu::Stval::read();
	f.type = Kernel::Thread_fault::PAGE_MISSING;
}


void Genode::Cpu::clear_memory_region(addr_t const addr,
                                      size_t const size, bool)
{
	memset((void*)addr, 0, size);

	/* FIXME: is this really necessary? */
	Genode::Cpu::sfence();
}
