/*
 * \brief   ARM cpu context initialization
 * \author  Stefan Kalkowski
 * \date    2017-04-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/bit_allocator.h>
#include <cpu/memory_barrier.h>
#include <base/internal/unmanaged_singleton.h>

#include <kernel/cpu.h>
#include <kernel/thread.h>
#include <spec/arm/cpu_support.h>

using namespace Genode;

Arm_cpu::Context::Context(bool privileged)
{
	using Psr = Arm_cpu::Psr;

	Psr::access_t v = 0;
	Psr::M::set(v, privileged ? Psr::M::SYS : Psr::M::USR);
	if (Board::Pic::fast_interrupts()) Psr::I::set(v, 1);
	else                               Psr::F::set(v, 1);
	Psr::A::set(v, 1);
	cpsr = v;
	cpu_exception = RESET;
}


using Asid_allocator = Bit_allocator<256>;

static Asid_allocator &alloc() {
	return *unmanaged_singleton<Asid_allocator>(); }


Arm_cpu::Mmu_context::Mmu_context(addr_t table)
: cidr((uint8_t)alloc().alloc()), ttbr0(Ttbr::init(table)) { }


Genode::Arm_cpu::Mmu_context::~Mmu_context()
{
	/* flush TLB by ASID */
	Cpu::Tlbiasid::write(id());
	alloc().free(id());
}


using Thread_fault = Kernel::Thread_fault;

void Arm_cpu::mmu_fault(Context & c, Thread_fault & fault)
{
	bool prefetch     = c.cpu_exception == Context::PREFETCH_ABORT;
	fault.addr        = prefetch ? Ifar::read() : Dfar::read();
	Fsr::access_t fsr = prefetch ? Ifsr::read() : Dfsr::read();

	if (!prefetch && Dfsr::Wnr::get(fsr)) {
		fault.type = Thread_fault::WRITE;
		return;
	}

	Cpu::mmu_fault_status(Fsr::Fs::get(fsr), fault);
}


void Arm_cpu::mmu_fault_status(Fsr::access_t fsr, Thread_fault & fault)
{
	enum {
		FAULT_MASK  = 0b11101,
		TRANSLATION = 0b00101,
		PERMISSION  = 0b01101,
	};

	switch(fsr & FAULT_MASK) {
		case TRANSLATION: fault.type = Thread_fault::PAGE_MISSING; return;
		case PERMISSION:  fault.type = Thread_fault::EXEC;         return;
		default:          fault.type = Thread_fault::UNKNOWN;
	};
}


void Arm_cpu::switch_to(Arm_cpu::Context&, Arm_cpu::Mmu_context & o)
{
	if (o.cidr == 0) return;

	Cidr::access_t cidr = Cidr::read();
	if (cidr != o.cidr) {
		/**
		 * First switch to global mappings only to prevent
		 * that wrong branch predicts result due to ASID
		 * and Page-Table not being in sync (see ARM RM B 3.10.4)
		 */
		Cidr::write(0);
		Cpu::synchronization_barrier();
		Ttbr0::write(o.ttbr0);
		Cpu::synchronization_barrier();
		Cidr::write(o.cidr);
		Cpu::synchronization_barrier();
	}
}


template <typename FUNC>
static inline void cache_maintainance(addr_t const base,
                                      size_t const size,
                                      size_t const cache_line_size,
                                      FUNC & func)
{
	/**
	 * Although, the ARMv7 reference manual states that addresses does not
	 * need to be cacheline aligned, we observed problems when not doing so
	 * on i.MX6 Quad Sabrelite (maybe Cortex A9 generic issue?).
	 * Therefore, we align it here.
	 */
	addr_t start     = base & ~(cache_line_size-1);
	addr_t const end = base + size;

	/* iterate over all cachelines of the given region and apply the functor */
	for (; start < end; start += cache_line_size) func(start);
}


void Arm_cpu::cache_coherent_region(addr_t const base,
                                    size_t const size)
{
	Genode::memory_barrier();

	/**
	 * according to ARMv7 ref. manual: clean lines from data-cache,
	 * invalidate line in instruction-cache and branch-predictor
	 */
	auto lambda = [] (addr_t const base) {
		Cpu::Dccmvac::write(base);
		Cpu::synchronization_barrier();
		Cpu::Icimvau::write(base);
		Cpu::Bpimva::write(base);
		Cpu::synchronization_barrier();
	};

	/* take the smallest cacheline, either from I-, or D-cache */
	size_t const cache_line_size = Genode::min(Cpu::instruction_cache_line_size(),
	                                           Cpu::data_cache_line_size());
	cache_maintainance(base, size, cache_line_size, lambda);
}


void Arm_cpu::clean_data_cache_by_virt_region(addr_t const base,
                                              size_t const size)
{
	auto lambda = [] (addr_t const base) { Dccmvac::write(base); };
	cache_maintainance(base, size, Cpu::data_cache_line_size(), lambda);
}


void Arm_cpu::clean_invalidate_data_cache_by_virt_region(addr_t const base,
                                                         size_t const size)
{
	auto lambda = [] (addr_t const base) { Dccimvac::write(base); };
	cache_maintainance(base, size, Cpu::data_cache_line_size(), lambda);
}


/**
 * Slightly more efficient method than Genode::memset,
 * using word-wise assignment
 */
static inline void memzero(addr_t const addr, size_t const size)
{
	if (align_addr(addr, 2) == addr && align_addr(size, 2) == size) {
		char * base = (char*) addr;
		unsigned count = size/4;
		for (; count--; base += 4) *((int*)base) = 0;
	} else {
		memset((void*)addr, 0, size);
	}
}


void Arm_cpu::clear_memory_region(addr_t const addr,
                                  size_t const size,
                                  bool changed_cache_properties)
{
	Genode::memory_barrier();

	memzero(addr, size);

	/**
	 * if the cache properties changed, this means we potentially zeroed
	 * DMA memory, which needs to be evicted from the D-cache
	 */
	if (changed_cache_properties) {
		Cpu::clean_invalidate_data_cache_by_virt_region(addr, size);
	}

	/**
	 * Invalidate the instruction cache, maybe lines from this memory are
	 * still within it.
	 */
	invalidate_instr_cache();
	Cpu::synchronization_barrier();
}
