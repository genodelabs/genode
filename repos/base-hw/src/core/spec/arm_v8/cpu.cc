/*
 * \brief   ARMv8 cpu context initialization
 * \author  Stefan Kalkowski
 * \date    2017-04-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <board.h>
#include <cpu.h>
#include <kernel/thread.h>
#include <util/bit_allocator.h>


Genode::Cpu::Context::Context(bool privileged)
{
	Spsr::El::set(pstate, privileged ? 1 : 0);
}


void Genode::Cpu::switch_to(Context&, Mmu_context & mmu_context)
{
	if (mmu_context.id() == 0) return;

	if (mmu_context.id() != Ttbr::Asid::get(Ttbr0_el1::read()))
		Ttbr0_el1::write(mmu_context.ttbr);
}


void Genode::Cpu::mmu_fault(Genode::Cpu::Context &,
                            Kernel::Thread_fault & fault)
{
	Esr::access_t esr = Esr_el1::read();

	fault.addr = Far_el1::read();

	switch (Esr::Iss::Abort::Fsc::get(Esr::Iss::get(esr))) {
	case Esr::Iss::Abort::Fsc::TRANSLATION:
		fault.type = Kernel::Thread_fault::PAGE_MISSING;
		return;
	case Esr::Iss::Abort::Fsc::PERMISSION:
		fault.type = Esr::Iss::Abort::Write::get(Esr::Iss::get(esr))
			? Kernel::Thread_fault::WRITE : Kernel::Thread_fault::EXEC;
		return;
	default:
		Genode::raw("MMU-fault not handled ESR=", Genode::Hex(esr));
		fault.type = Kernel::Thread_fault::UNKNOWN;
	};
}


using Asid_allocator = Genode::Bit_allocator<65536>;

static Asid_allocator &alloc() {
	return *unmanaged_singleton<Asid_allocator>(); }


Genode::Cpu::Mmu_context::Mmu_context(addr_t table)
: ttbr(Ttbr::Baddr::masked(table))
{
	Ttbr::Asid::set(ttbr, (Genode::uint16_t)alloc().alloc());
}


Genode::Cpu::Mmu_context::~Mmu_context()
{
	alloc().free(id());
}


static constexpr Genode::addr_t line_size = 1 << Board::CACHE_LINE_SIZE_LOG2;
static constexpr Genode::addr_t line_align_mask = ~(line_size - 1);


void Genode::Cpu::clean_data_cache_by_virt_region(addr_t base, size_t sz)
{
	addr_t const top = base + sz;
	base &= line_align_mask;
	for (; base < top; base += line_size) {
		asm volatile("dc cvau, %0" :: "r" (base)); }
}


void Genode::Cpu::invalidate_instr_cache_by_virt_region(addr_t base,
                                                        size_t size)
{
	addr_t const top = base + size;
	base &= line_align_mask;
	for (; base < top; base += line_size) {
		asm volatile("ic ivau, %0" :: "r" (base)); }
}
