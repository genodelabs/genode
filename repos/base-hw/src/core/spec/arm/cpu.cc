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
