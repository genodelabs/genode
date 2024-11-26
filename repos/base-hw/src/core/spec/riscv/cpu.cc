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

/* base-hw core includes */
#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>

using Mmu_context = Core::Cpu::Mmu_context;

using namespace Core;


void Cpu::Context::print(Output &output) const
{
	using namespace Genode;
	using Genode::print;

	print(output, "\n");
	print(output, "  ip     = ", Hex(ip),  "\n");
	print(output, "  ra     = ", Hex(ra),  "\n");
	print(output, "  sp     = ", Hex(sp),  "\n");
	print(output, "  gp     = ", Hex(gp),  "\n");
	print(output, "  tp     = ", Hex(tp),  "\n");
	print(output, "  t0     = ", Hex(t0),  "\n");
	print(output, "  t1     = ", Hex(t1),  "\n");
	print(output, "  t2     = ", Hex(t2),  "\n");
	print(output, "  s0     = ", Hex(s0),  "\n");
	print(output, "  s1     = ", Hex(s1),  "\n");
	print(output, "  a0     = ", Hex(a0),  "\n");
	print(output, "  a1     = ", Hex(a1),  "\n");
	print(output, "  a2     = ", Hex(a2),  "\n");
	print(output, "  a3     = ", Hex(a3),  "\n");
	print(output, "  a4     = ", Hex(a4),  "\n");
	print(output, "  a5     = ", Hex(a5),  "\n");
	print(output, "  a6     = ", Hex(a6),  "\n");
	print(output, "  a7     = ", Hex(a7),  "\n");
	print(output, "  s2     = ", Hex(s2),  "\n");
	print(output, "  s3     = ", Hex(s3),  "\n");
	print(output, "  s4     = ", Hex(s4),  "\n");
	print(output, "  s5     = ", Hex(s5),  "\n");
	print(output, "  s6     = ", Hex(s6),  "\n");
	print(output, "  s7     = ", Hex(s7),  "\n");
	print(output, "  s8     = ", Hex(s8),  "\n");
	print(output, "  s9     = ", Hex(s9),  "\n");
	print(output, "  s10    = ", Hex(s10), "\n");
	print(output, "  s11    = ", Hex(s11), "\n");
	print(output, "  t3     = ", Hex(t3),  "\n");
	print(output, "  t4     = ", Hex(t4),  "\n");
	print(output, "  t5     = ", Hex(t5),  "\n");
	print(output, "  t6     = ", Hex(t6));
}


Cpu::Context::Context(bool)
{
	/*
	 * initialize cpu_exception with something that gets ignored in
	 * Thread::exception
	 */
	cpu_exception = IRQ_FLAG;
}


Mmu_context::Mmu_context(addr_t page_table_base,
                         Board::Address_space_id_allocator &id_alloc)
:
	_addr_space_id_alloc(id_alloc)
{
	Satp::Asid::set(satp, (uint8_t)_addr_space_id_alloc.alloc());
	Satp::Ppn::set(satp, page_table_base >> 12);
	Satp::Mode::set(satp, 8);
}


Mmu_context::~Mmu_context()
{
	unsigned asid = (uint16_t)Satp::Asid::get(satp); /* ASID is 16 bit */
	Cpu::invalidate_tlb_by_pid(asid);
	_addr_space_id_alloc.free(asid);
}


bool Cpu::active(Mmu_context &context)
{
	return Satp::read() == context.satp;
}


void Cpu::switch_to(Mmu_context &context)
{
	Satp::write(context.satp);
	sfence();
}


void Cpu::mmu_fault(Context &, Kernel::Thread_fault &f)
{
	f.addr = Cpu::Stval::read();
	f.type = Kernel::Thread_fault::PAGE_MISSING;
}


void Cpu::clear_memory_region(addr_t const addr, size_t const size, bool)
{
	memset((void*)addr, 0, size);

	/* FIXME: is this really necessary? */
	Cpu::sfence();
}
