/*
 * \brief   Parts of platform that are specific to Arndale
 * \author  Martin Stein
 * \date    2012-04-27
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

extern "C" void *    _start_setup_stack;   /* entrypoint for non-boot CPUs */
static unsigned char hyp_mode_stack[1024]; /* hypervisor mode's kernel stack */

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE }),
  core_mmio(Memory_region { IRQ_CONTROLLER_BASE, IRQ_CONTROLLER_SIZE },
            Memory_region { IRQ_CONTROLLER_VT_CTRL_BASE, IRQ_CONTROLLER_VT_CTRL_SIZE },
            Memory_region { MCT_MMIO_BASE, MCT_MMIO_SIZE },
            Memory_region { UART_2_MMIO_BASE, UART_2_MMIO_SIZE }) { }


static inline void prepare_nonsecure_world()
{
	using Cpu = Hw::Arm_cpu;

	/* if we are already in HYP mode we're done (depends on u-boot version) */
	if (Cpu::Psr::M::get(Cpu::Cpsr::read()) == Cpu::Psr::M::HYP)
		return;

	/* ARM generic timer counter freq needs to be set in secure mode */
	volatile unsigned long * mct_control = (unsigned long*) 0x101C0240;
	*mct_control = 0x100;
	Cpu::Cntfrq::write(24000000);

	/*
	 * enable coprocessor 10 + 11 access and SMP bit access in auxiliary control
	 * register for non-secure world
	 */
	Cpu::Nsacr::access_t nsacr = 0;
	Cpu::Nsacr::Cpnsae10::set(nsacr, 1);
	Cpu::Nsacr::Cpnsae11::set(nsacr, 1);
	Cpu::Nsacr::Ns_smp::set(nsacr, 1);
	Cpu::Nsacr::write(nsacr);

	asm volatile (
		"msr sp_mon, sp \n" /* copy current mode's sp */
		"msr lr_mon, lr \n" /* copy current mode's lr */
		"cps #22        \n" /* switch to monitor mode */
		);

	Cpu::Scr::access_t scr = 0;
	Cpu::Scr::Ns::set(scr,  1);
	Cpu::Scr::Fw::set(scr,  1);
	Cpu::Scr::Aw::set(scr,  1);
	Cpu::Scr::Scd::set(scr, 1);
	Cpu::Scr::Hce::set(scr, 1);
	Cpu::Scr::Sif::set(scr, 1);
	Cpu::Scr::write(scr);
}


static inline void prepare_hypervisor(Genode::addr_t table)
{
	using Cpu = Hw::Arm_cpu;

	/* set hypervisor exception vector */
	Cpu::Hvbar::write(0xfff00000); /* FIXME */

	/* set hypervisor's translation table */
	Cpu::Httbr_64bit::write(table);

	Cpu::Ttbcr::access_t ttbcr = 0;
	Cpu::Ttbcr::Irgn0::set(ttbcr, 1);
	Cpu::Ttbcr::Orgn0::set(ttbcr, 1);
	Cpu::Ttbcr::Sh0::set(ttbcr, 2);
	Cpu::Ttbcr::Eae::set(ttbcr, 1);

	/* prepare MMU usage by hypervisor code */
	Cpu::Htcr::write(ttbcr);

	/* don't trap on cporocessor 10 + 11, but all others */
	Cpu::Hcptr::access_t hcptr = 0;
	Cpu::Hcptr::Tcp<0>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<1>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<2>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<3>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<4>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<5>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<6>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<7>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<8>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<9>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<12>::set(hcptr, 1);
	Cpu::Hcptr::Tcp<13>::set(hcptr, 1);
	Cpu::Hcptr::Tta::set(hcptr, 1);
	Cpu::Hcptr::Tcpac::set(hcptr, 1);
	Cpu::Hcptr::write(hcptr);

	enum Memory_attributes {
		DEVICE_MEMORY          = 0x04,
		NORMAL_MEMORY_UNCACHED = 0x44,
		NORMAL_MEMORY_CACHED   = 0xff,
	};

	Cpu::Mair0::access_t mair0 = 0;
	Cpu::Mair0::Attr0::set(mair0, NORMAL_MEMORY_UNCACHED);
	Cpu::Mair0::Attr1::set(mair0, DEVICE_MEMORY);
	Cpu::Mair0::Attr2::set(mair0, NORMAL_MEMORY_CACHED);
	Cpu::Mair0::Attr3::set(mair0, DEVICE_MEMORY);
	Cpu::Hmair0::write(mair0);

	Cpu::Vtcr::access_t vtcr = ttbcr;
	Cpu::Vtcr::Sl0::set(vtcr, 1); /* set to starting level 1 */
	Cpu::Vtcr::write(vtcr);

	Cpu::Sctlr::access_t sctlr = Cpu::Sctlr::read();
	Cpu::Sctlr::C::set(sctlr, 1);
	Cpu::Sctlr::I::set(sctlr, 1);
	Cpu::Sctlr::V::set(sctlr, 1);
	Cpu::Sctlr::A::set(sctlr, 0);
	Cpu::Sctlr::M::set(sctlr, 1);
	Cpu::Sctlr::Z::set(sctlr, 1);
	Cpu::Hsctlr::write(sctlr);
}


static inline void switch_to_supervisor_mode()
{
	using Cpsr = Hw::Arm_cpu::Psr;

	Cpsr::access_t cpsr = 0;
	Cpsr::M::set(cpsr, Cpsr::M::SVC);
	Cpsr::F::set(cpsr, 1);
	Cpsr::I::set(cpsr, 1);

	asm volatile (
		"msr sp_svc, sp        \n" /* copy current mode's sp           */
		"msr lr_svc, lr        \n" /* copy current mode's lr           */
		"msr elr_hyp, lr       \n" /* copy current mode's lr to hyp lr */
		"msr sp_hyp, %[stack]  \n" /* copy to hyp stack pointer        */
		"msr spsr_cxfs, %[cpsr] \n" /* set psr for supervisor mode      */
		"adr lr, 1f            \n" /* load exception return address    */
		"eret                  \n" /* exception return                 */
		"1:":: [cpsr] "r" (cpsr), [stack] "r" (&hyp_mode_stack));
}


void Bootstrap::Platform::enable_mmu()
{
	static volatile bool primary_cpu = true;
	pic.init_cpu_local();

	prepare_nonsecure_world();
	prepare_hypervisor((addr_t)core_pd->table_base);
	switch_to_supervisor_mode();

	Cpu::Sctlr::init();
	Cpu::Cpsr::init();

	cpu.invalidate_data_cache();

	/* primary cpu wakes up all others */
	if (primary_cpu && NR_OF_CPUS > 1) {
		primary_cpu = false;
		cpu.wake_up_all_cpus(&_start_setup_stack);
	}

	cpu.enable_mmu_and_caches((Genode::addr_t)core_pd->table_base);
}


void Bootstrap::Cpu::wake_up_all_cpus(void * const ip)
{
	*(void * volatile *)Board::IRAM_BASE = ip;
	asm volatile("dsb; sev;");
}
