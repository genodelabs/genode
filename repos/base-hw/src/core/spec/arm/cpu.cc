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
#include <spec/arm/cpu_support.h>

Genode::Arm_cpu::Context::Context(bool privileged)
{
	using Psr = Arm_cpu::Psr;

	Psr::access_t v = 0;
	Psr::M::set(v, privileged ? Psr::M::SYS : Psr::M::USR);
	if (Genode::Pic::fast_interrupts()) Psr::I::set(v, 1);
	else                                Psr::F::set(v, 1);
	Psr::A::set(v, 1);
	cpsr = v;
	cpu_exception = RESET;
}


using Asid_allocator = Genode::Bit_allocator<256>;

static Asid_allocator &alloc() {
	return *unmanaged_singleton<Asid_allocator>(); }


Genode::Arm_cpu::Mmu_context::Mmu_context(addr_t table)
: cidr((Genode::uint8_t)alloc().alloc()), ttbr0(Ttbr0::init(table)) { }


Genode::Arm_cpu::Mmu_context::~Mmu_context()
{
	/* flush TLB by ASID */
	Cpu::Tlbiasid::write(id());
	alloc().free(id());
}
