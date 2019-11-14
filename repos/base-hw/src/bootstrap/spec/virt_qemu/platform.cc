/*
 * \brief   Platform implementations specific for base-hw and Qemu arm virt machine
 * \author  Piotr Tworek
 * \date    2019-09-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <spec/arm/cortex_a7_a15_virtualization.h>

extern "C" void *    _start_setup_stack;   /* entrypoint for non-boot CPUs */
static unsigned char hyp_mode_stack[1024]; /* hypervisor mode's kernel stack */

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_BASE, RAM_SIZE }),
  late_ram_regions(Memory_region { }),
  core_mmio(Memory_region { UART_BASE, UART_SIZE },
            Memory_region { Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE,
                            Cpu_mmio::IRQ_CONTROLLER_DISTR_SIZE },
            Memory_region { Cpu_mmio::IRQ_CONTROLLER_CPU_BASE,
                            Cpu_mmio::IRQ_CONTROLLER_CPU_SIZE },
            Memory_region { Cpu_mmio::IRQ_CONTROLLER_VT_CTRL_BASE,
                            Cpu_mmio::IRQ_CONTROLLER_VT_CTRL_SIZE }) {}


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
		"adr lr, 1f            \n" /* load exception return address    */
		"msr elr_hyp, lr       \n" /* copy current mode's lr to hyp lr */
		"mov sp, %[stack]      \n" /* copy to hyp stack pointer        */
		"msr spsr_cxfs, %[cpsr] \n" /* set psr for supervisor mode      */
		"eret                  \n" /* exception return                 */
		"1:":: [cpsr] "r" (cpsr), [stack] "r" (&hyp_mode_stack));
}


unsigned Bootstrap::Platform::enable_mmu()
{
	static volatile bool primary_cpu = true;

	/* locally initialize interrupt controller */
	::Board::Pic pic { };

	/* primary cpu wakes up all others */
	if (primary_cpu && NR_OF_CPUS > 1) {
		Cpu::invalidate_data_cache();
		primary_cpu = false;
		Cpu::wake_up_all_cpus(&_start_setup_stack);
	}

	prepare_hypervisor((addr_t)core_pd->table_base);
	switch_to_supervisor_mode();

	Cpu::Sctlr::init();
	Cpu::Cpsr::init();

	Cpu::enable_mmu_and_caches((Genode::addr_t)core_pd->table_base);

	return Cpu::Mpidr::Aff_0::get(Cpu::Mpidr::read());
}


void Board::Cpu::wake_up_all_cpus(void * const ip)
{
	for (unsigned cpu_id = 1; cpu_id < NR_OF_CPUS; cpu_id++) {
		if (!Board::Psci::cpu_on(cpu_id, ip)) {
			Genode::error("Failed to boot CPU", cpu_id);
		}
	}
}
