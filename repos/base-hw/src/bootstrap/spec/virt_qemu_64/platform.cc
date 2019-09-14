/*
 * \brief   Platform implementations specific for base-hw and Qemu arm64 virt machine
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

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { ::Board::RAM_BASE, ::Board::RAM_SIZE }),
  late_ram_regions(Memory_region { }),
  core_mmio(Memory_region { ::Board::UART_BASE, ::Board::UART_SIZE },
            Memory_region { ::Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE,
                            ::Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_SIZE },
            Memory_region { ::Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE,
                            ::Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE })
{
	::Board::Pic pic {};
}


void Board::Cpu::wake_up_all_cpus(void *entry)
{
	for (unsigned cpu_id = 1; cpu_id < NR_OF_CPUS; cpu_id++) {
		if (!Board::Psci::cpu_on(cpu_id, entry)) {
			Genode::error("Failed to boot CPU", cpu_id);
		}
	}
}
