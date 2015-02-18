/*
 * \brief  Board-specific code for Arndale
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <board.h>
#include <cpu.h>

/* hypervisor exception vector address */
extern void* _hyp_kernel_entry;


static inline void prepare_nonsecure_world()
{
	using Nsacr = Genode::Cpu::Nsacr;
	using Cpsr  = Genode::Cpu::Psr;
	using Scr   = Genode::Cpu::Scr;

	/* if we are already in HYP mode we're done (depends on u-boot version) */
	if (Cpsr::M::get(Cpsr::read()) == Cpsr::M::HYP)
		return;

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
		"msr spsr_cxfs, %[psr] \n" /* set psr for supervisor mode      */
		"eret                  \n" /* exception return                 */
		:: [psr] "r" (psr));
}


void Genode::Board::prepare_kernel()
{
	prepare_nonsecure_world();
	Genode::Cpu::hyp_exception_entry_at(&_hyp_kernel_entry);
	switch_to_supervisor_mode();
}
