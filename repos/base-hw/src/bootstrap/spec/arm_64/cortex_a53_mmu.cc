/*
 * \brief   Platform implementations specific for Cortex A53 CPUs
 * \author  Stefan Kalkowski
 * \date    2019-05-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <hw/spec/arm_64/memory_map.h>
#include <platform.h>

using Board::Cpu;

extern "C" void * _crt0_start_secondary;

static inline void prepare_non_secure_world()
{
	bool el2 = Cpu::Id_pfr0::El2::get(Cpu::Id_pfr0::read());

	Cpu::Scr::access_t scr = Cpu::Scr::read();
	Cpu::Scr::Ns::set(scr,  1); /* set non-secure bit */
	Cpu::Scr::Rw::set(scr,  1); /* exec in aarch64    */
	Cpu::Scr::Smd::set(scr, 1); /* disable smc call   */
	Cpu::Scr::write(scr);

	Cpu::Spsr::access_t pstate = 0;
	Cpu::Spsr::Sp::set(pstate, 1); /* select non-el0 stack pointer */
	Cpu::Spsr::El::set(pstate, el2 ? Cpu::Current_el::EL2
	                               : Cpu::Current_el::EL1);
	Cpu::Spsr::F::set(pstate, 1);
	Cpu::Spsr::I::set(pstate, 1);
	Cpu::Spsr::A::set(pstate, 1);
	Cpu::Spsr::D::set(pstate, 1);
	Cpu::Spsr_el3::write(pstate);

#ifndef SWITCH_TO_ELX
#define SWITCH_TO_ELX(el)   \
	"mov x0, sp \n"         \
	"msr sp_" #el ", x0 \n" \
	"adr x0, 1f \n"         \
	"msr elr_el3, x0 \n"    \
	"eret \n"               \
	"1:"

	if (el2)
		asm volatile(SWITCH_TO_ELX(el2) ::: "x0");
	else
		asm volatile(SWITCH_TO_ELX(el1) ::: "x0");
#undef SWITCH_TO_ELX
#else
#error "macro SWITCH_TO_ELX already defined"
#endif
}


static inline void prepare_hypervisor(Cpu::Ttbr::access_t const ttbr)
{
	using namespace Hw::Mm;

	/* forbid trace access */
	Cpu::Cptr_el2::access_t cptr = Cpu::Cptr_el2::read();
	Cpu::Cptr_el2::Tta::set(cptr, 1);
	Cpu::Cptr_el2::write(cptr);

	/* allow physical counter/timer access without trapping */
	Cpu::Cnthctl_el2::write(0b111);

	/* forbid any 32bit access to coprocessor/sysregs */
	Cpu::Hstr_el2::write(0xffff);

	Cpu::Hcr_el2::access_t hcr = Cpu::Hcr_el2::read();
	Cpu::Hcr_el2::Rw::set(hcr, 1); /* exec in aarch64 */
	Cpu::Hcr_el2::write(hcr);

	/* set hypervisor exception vector */
	Cpu::Vbar_el2::write(el2_addr(hypervisor_exception_vector().base));
	Genode::addr_t const stack_el2 = el2_addr(hypervisor_stack().base +
	                                          hypervisor_stack().size);

	/* set hypervisor's translation table */
	Cpu::Ttbr0_el2::write(ttbr);

	Cpu::Tcr_el2::access_t tcr_el2 = 0;
	Cpu::Tcr_el2::T0sz::set(tcr_el2, 25);
	Cpu::Tcr_el2::Irgn0::set(tcr_el2, 1);
	Cpu::Tcr_el2::Orgn0::set(tcr_el2, 1);
	Cpu::Tcr_el2::Sh0::set(tcr_el2, 0b10);

	/* prepare MMU usage by hypervisor code */
	Cpu::Tcr_el2::write(tcr_el2);

	/* set memory attributes in indirection register */
	Cpu::Mair::access_t mair = 0;
	Cpu::Mair::Attr0::set(mair, Cpu::Mair::NORMAL_MEMORY_UNCACHED);
	Cpu::Mair::Attr1::set(mair, Cpu::Mair::DEVICE_MEMORY);
	Cpu::Mair::Attr2::set(mair, Cpu::Mair::NORMAL_MEMORY_CACHED);
	Cpu::Mair::Attr3::set(mair, Cpu::Mair::DEVICE_MEMORY);
	Cpu::Mair_el2::write(mair);

	Cpu::Vtcr_el2::access_t vtcr = 0;
	Cpu::Vtcr_el2::T0sz::set(vtcr, 25);
	Cpu::Vtcr_el2::Sl0::set(vtcr, 1); /* set to starting level 1 */
	Cpu::Vtcr_el2::write(vtcr);

	Cpu::Spsr::access_t pstate = 0;
	Cpu::Spsr::Sp::set(pstate, 1); /* select non-el0 stack pointer */
	Cpu::Spsr::El::set(pstate, Cpu::Current_el::EL1);
	Cpu::Spsr::F::set(pstate, 1);
	Cpu::Spsr::I::set(pstate, 1);
	Cpu::Spsr::A::set(pstate, 1);
	Cpu::Spsr::D::set(pstate, 1);
	Cpu::Spsr_el2::write(pstate);

	Cpu::Sctlr::access_t sctlr = Cpu::Sctlr_el2::read();
	Cpu::Sctlr::M::set(sctlr, 1);
	Cpu::Sctlr::A::set(sctlr, 0);
	Cpu::Sctlr::C::set(sctlr, 1);
	Cpu::Sctlr::Sa::set(sctlr, 0);
	Cpu::Sctlr::I::set(sctlr, 1);
	Cpu::Sctlr::Wxn::set(sctlr, 0);
	Cpu::Sctlr_el2::write(sctlr);

	asm volatile("mov x0, sp      \n"
	             "msr sp_el1, x0  \n"
	             "adr x0, 1f      \n"
	             "msr elr_el2, x0 \n"
	             "mov sp, %0      \n"
	             "eret            \n"
	             "1:": : "r"(stack_el2): "x0");
}


unsigned Bootstrap::Platform::enable_mmu()
{
	static volatile bool primary_cpu = true;
	bool primary = primary_cpu;
	if (primary) primary_cpu = false;

	Cpu::Ttbr::access_t ttbr =
		Cpu::Ttbr::Baddr::masked((Genode::addr_t)core_pd->table_base);

	/* primary cpu wakes up all others */
	if (primary && NR_OF_CPUS > 1) Cpu::wake_up_all_cpus(&_crt0_start_secondary);

	while (Cpu::current_privilege_level() > Cpu::Current_el::EL1) {
		if (Cpu::current_privilege_level() == Cpu::Current_el::EL3) {
			prepare_non_secure_world();
		} else {
			::Board::Pic pic __attribute__((unused)) {};
			prepare_hypervisor(ttbr);
		}
	}

	/* enable performance counter for user-land */
	Cpu::Pmuserenr_el0::write(0b1111);
	Cpu::Pmcr_el0::access_t pmcr = Cpu::Pmcr_el0::read();
	Cpu::Pmcr_el0::write(pmcr | 1);
	Cpu::Pmcntenset_el0::write(1 << 31);

	/* enable user-level access of physical/virtual counter */
	Cpu::Cntkctl_el1::write(0b11);

	Cpu::Vbar_el1::write(Hw::Mm::supervisor_exception_vector().base);

	/* set memory attributes in indirection register */
	Cpu::Mair::access_t mair = 0;
	Cpu::Mair::Attr0::set(mair, Cpu::Mair::NORMAL_MEMORY_UNCACHED);
	Cpu::Mair::Attr1::set(mair, Cpu::Mair::DEVICE_MEMORY);
	Cpu::Mair::Attr2::set(mair, Cpu::Mair::NORMAL_MEMORY_CACHED);
	Cpu::Mair::Attr3::set(mair, Cpu::Mair::DEVICE_MEMORY);
	Cpu::Mair_el1::write(mair);

	Cpu::Ttbr0_el1::write(ttbr);
	Cpu::Ttbr1_el1::write(ttbr);

	Cpu::Tcr_el1::access_t tcr = 0;
	Cpu::Tcr_el1::T0sz::set(tcr, 25);
	Cpu::Tcr_el1::T1sz::set(tcr, 25);
	Cpu::Tcr_el1::Irgn0::set(tcr, 1);
	Cpu::Tcr_el1::Irgn1::set(tcr, 1);
	Cpu::Tcr_el1::Orgn0::set(tcr, 1);
	Cpu::Tcr_el1::Orgn1::set(tcr, 1);
	Cpu::Tcr_el1::Sh0::set(tcr, 0b10);
	Cpu::Tcr_el1::Sh1::set(tcr, 0b10);
	Cpu::Tcr_el1::Ips::set(tcr, 0b10);
	Cpu::Tcr_el1::As::set(tcr, 1);
	Cpu::Tcr_el1::write(tcr);

	Cpu::Sctlr::access_t sctlr = Cpu::Sctlr_el1::read();
	Cpu::Sctlr::C::set(sctlr, 1);
	Cpu::Sctlr::I::set(sctlr, 1);
	Cpu::Sctlr::A::set(sctlr, 0);
	Cpu::Sctlr::M::set(sctlr, 1);
	Cpu::Sctlr::Sa0::set(sctlr, 1);
	Cpu::Sctlr::Sa::set(sctlr, 0);
	Cpu::Sctlr::Uct::set(sctlr, 1);
	Cpu::Sctlr_el1::write(sctlr);

	return (Cpu::Mpidr::read() & 0xff);
}
