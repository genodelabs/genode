/*
 * \brief  CPU driver
 * \author Stefan Kalkowski
 * \date   2017-10-06
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/unmanaged_singleton.h>

#include <hw/assert.h>
#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>

using Mmu_context = Genode::Cpu::Mmu_context;
using Asid_allocator = Genode::Bit_allocator<256>;


Genode::Cpu::Context::Context(bool)
{
	/*
	 * initialize cpu_exception with something that gets ignored in
	 * Thread::exception
	 */
	cpu_exception = IRQ_FLAG;
}


static Asid_allocator & alloc() {
	return *unmanaged_singleton<Asid_allocator>(); }


Mmu_context::Mmu_context(addr_t page_table_base)
{
	Sptbr::Asid::set(sptbr, (Genode::uint8_t)alloc().alloc());
	Sptbr::Ppn::set(sptbr, page_table_base >> 12);
}


Mmu_context::~Mmu_context()
{
	unsigned asid = Sptbr::Asid::get(sptbr);
	Cpu::invalidate_tlb_by_pid(asid);
	alloc().free(asid);
}


void Genode::Cpu::switch_to(Mmu_context & context)
{
	/*
	 * The sstatus register defines to which privilege level
	 * the machin returns when doing an exception return
	 */
	bool user = Sptbr::Asid::get(context.sptbr);
	Sstatus::access_t v = Sstatus::read();
	Sstatus::Spp::set(v, user ? 0 : 1);
	Sstatus::write(v);

	/* change the translation table when necessary */
	//Sptbr::access_t sptbr = Sptbr::read();
	if (user /*&& sptbr != context.sptbr*/)
		Sptbr::write(context.sptbr);
}


void Genode::Cpu::mmu_fault(Context &, Kernel::Thread_fault & f)
{
	f.addr = Genode::Cpu::Sbadaddr::read();
	f.type = Kernel::Thread_fault::PAGE_MISSING;
}


void Genode::Cpu::clear_memory_region(addr_t const addr,
                                      size_t const size, bool)
{
	memset((void*)addr, 0, size);

	/* FIXME: is this really necessary? */
	Genode::Cpu::sfence();
}
