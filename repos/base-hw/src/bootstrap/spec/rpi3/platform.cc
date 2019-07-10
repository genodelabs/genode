/*
 * \brief   Platform implementations specific for base-hw and Raspberry Pi3
 * \author  Stefan Kalkowski
 * \date    2019-05-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

using Board::Cpu;


/**
 * Leave out the first page (being 0x0) from bootstraps RAM allocator,
 * some code does not feel happy with addresses being zero
 */
Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { ::Board::RAM_BASE + 0x1000,
                                    ::Board::RAM_SIZE - 0x1000 }),
  late_ram_regions(Memory_region { ::Board::RAM_BASE, 0x1000 }),
  core_mmio(Memory_region { ::Board::UART_BASE, ::Board::UART_SIZE },
            Memory_region { ::Board::LOCAL_IRQ_CONTROLLER_BASE,
                            ::Board::LOCAL_IRQ_CONTROLLER_SIZE },
            Memory_region { ::Board::IRQ_CONTROLLER_BASE,
                            ::Board::IRQ_CONTROLLER_SIZE }) {}


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


static inline void prepare_hypervisor()
{
	Cpu::Hcr::access_t scr = Cpu::Hcr::read();
	Cpu::Hcr::Rw::set(scr,  1); /* exec in aarch64 */
	Cpu::Hcr::write(scr);

	Cpu::Spsr::access_t pstate = 0;
	Cpu::Spsr::Sp::set(pstate, 1); /* select non-el0 stack pointer */
	Cpu::Spsr::El::set(pstate, Cpu::Current_el::EL1);
	Cpu::Spsr::F::set(pstate, 1);
	Cpu::Spsr::I::set(pstate, 1);
	Cpu::Spsr::A::set(pstate, 1);
	Cpu::Spsr::D::set(pstate, 1);
	Cpu::Spsr_el2::write(pstate);

	asm volatile("mov x0, sp      \n"
	             "msr sp_el1, x0  \n"
	             "adr x0, 1f      \n"
	             "msr elr_el2, x0 \n"
	             "eret            \n"
	             "1:");
}


unsigned Bootstrap::Platform::enable_mmu()
{
	while (Cpu::current_privilege_level() > Cpu::Current_el::EL1) {
		if (Cpu::current_privilege_level() == Cpu::Current_el::EL3)
			prepare_non_secure_world();
		else
			prepare_hypervisor();
	}

	Cpu::Vbar_el1::write(Hw::Mm::supervisor_exception_vector().base);

	/* set memory attributes in indirection register */
	Cpu::Mair::access_t mair = 0;
	Cpu::Mair::Attr0::set(mair, Cpu::Mair::NORMAL_MEMORY_UNCACHED);
	Cpu::Mair::Attr1::set(mair, Cpu::Mair::DEVICE_MEMORY);
	Cpu::Mair::Attr2::set(mair, Cpu::Mair::NORMAL_MEMORY_CACHED);
	Cpu::Mair::Attr3::set(mair, Cpu::Mair::DEVICE_MEMORY);
	Cpu::Mair::write(mair);

	Cpu::Ttbr::access_t ttbr = Cpu::Ttbr::Baddr::masked((Genode::addr_t)core_pd->table_base);
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

	Cpu::Sctlr_el1::access_t sctlr = Cpu::Sctlr_el1::read();
	Cpu::Sctlr_el1::C::set(sctlr, 1);
	Cpu::Sctlr_el1::I::set(sctlr, 1);
	Cpu::Sctlr_el1::A::set(sctlr, 0);
	Cpu::Sctlr_el1::M::set(sctlr, 1);
	Cpu::Sctlr_el1::Sa0::set(sctlr, 1);
	Cpu::Sctlr_el1::Sa::set(sctlr, 0);
	Cpu::Sctlr_el1::write(sctlr);

	return 0;
}
