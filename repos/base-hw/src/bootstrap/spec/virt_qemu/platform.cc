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

extern "C" void * _start_setup_stack;

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_BASE, RAM_SIZE }),
  late_ram_regions(Memory_region { }),
  core_mmio(Memory_region { UART_BASE, UART_SIZE },
            Memory_region { Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE,
                            Cpu_mmio::IRQ_CONTROLLER_DISTR_SIZE },
            Memory_region { Cpu_mmio::IRQ_CONTROLLER_CPU_BASE,
                            Cpu_mmio::IRQ_CONTROLLER_CPU_SIZE }) {}


unsigned Bootstrap::Platform::enable_mmu()
{
	static volatile bool primary_cpu = true;

	/* locally initialize interrupt controller */
	::Board::Pic pic { };

	Cpu::Sctlr::init();
	Cpu::Cpsr::init();

        /* primary cpu wakes up all others */
        if (primary_cpu && NR_OF_CPUS > 1) {
                Cpu::invalidate_data_cache();
                primary_cpu = false;
                Cpu::wake_up_all_cpus(&_start_setup_stack);
        }

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
