/*
 * \brief  CPU-specific initialization code for Arndale
 * \author Stefan Kalkowski
 * \date   2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <cpu.h>
#include <translation_table.h>

static unsigned char hyp_mode_stack[1024];


static inline void prepare_nonsecure_world()
{
	using Nsacr = Genode::Cpu::Nsacr;
	using Cpsr  = Genode::Cpu::Psr;
	using Scr   = Genode::Cpu::Scr;

	/* if we are already in HYP mode we're done (depends on u-boot version) */
	if (Cpsr::M::get(Cpsr::read()) == Cpsr::M::HYP)
		return;

	/* ARM generic timer counter freq needs to be set in secure mode */
	volatile unsigned long * mct_control = (unsigned long*) 0x101C0240;
	*mct_control = 0x100;
	asm volatile ("mcr p15, 0, %0, c14, c0, 0" :: "r" (24000000));

	/*
	 * enable coprocessor 10 + 11 access and SMP bit access in auxiliary control
	 * register for non-secure world
	 */
	Nsacr::access_t nsacr = 0;
	Nsacr::Cpnsae10::set(nsacr, 1);
	Nsacr::Cpnsae11::set(nsacr, 1);
	Nsacr::Ns_smp::set(nsacr, 1);
	Nsacr::write(nsacr);

	asm volatile (
		"msr sp_mon, sp \n" /* copy current mode's sp */
		"msr lr_mon, lr \n" /* copy current mode's lr */
		"cps #22        \n" /* switch to monitor mode */
		);

	Scr::access_t scr = 0;
	Scr::Ns::set(scr,  1);
	Scr::Fw::set(scr,  1);
	Scr::Aw::set(scr,  1);
	Scr::Scd::set(scr, 1);
	Scr::Hce::set(scr, 1);
	Scr::Sif::set(scr, 1);
	Scr::write(scr);
}


static inline void prepare_hypervisor(Genode::Translation_table & table)
{
	using Genode::Cpu;

	/* set hypervisor exception vector */
	Cpu::hyp_exception_entry_at((void*)0xfff00000); /* FIXME */

	/* set hypervisor's translation table */
	Cpu::Httbr::translation_table((Genode::addr_t)&table);

	/* prepare MMU usage by hypervisor code */
	Cpu::Htcr::write(Cpu::Ttbcr::init_virt_kernel());
	Cpu::Hcptr::write(Cpu::Hcptr::init());
	Cpu::Hmair0::write(Cpu::Mair0::init_virt_kernel());
	Cpu::Vtcr::write(Cpu::Vtcr::init());
	Cpu::Hsctlr::write(Cpu::Sctlr::init_value());
}


static inline void switch_to_supervisor_mode()
{
	using Psr = Genode::Cpu::Psr;

	Psr::access_t psr = 0;
	Psr::M::set(psr, Psr::M::SVC);
	Psr::F::set(psr, 1);
	Psr::I::set(psr, 1);

	asm volatile (
		"msr sp_svc, sp        \n" /* copy current mode's sp           */
		"msr lr_svc, lr        \n" /* copy current mode's lr           */
		"msr elr_hyp, lr       \n" /* copy current mode's lr to hyp lr */
		"msr sp_hyp, %[stack]  \n" /* copy to hyp stack pointer        */
		"msr spsr_cxfs, %[psr] \n" /* set psr for supervisor mode      */
		"adr lr, 1f            \n" /* load exception return address    */
		"eret                  \n" /* exception return                 */
		"1:":: [psr] "r" (psr), [stack] "r" (&hyp_mode_stack));
}


void Genode::Cpu::init(Genode::Translation_table & table)
{
	prepare_nonsecure_world();
	prepare_hypervisor(table);
	switch_to_supervisor_mode();
}
