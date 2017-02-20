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

/* entrypoint for non-boot CPUs */
extern "C" void * _start_setup_stack;

Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE }),
  core_mmio(Memory_region { IRQ_CONTROLLER_BASE, IRQ_CONTROLLER_SIZE },
            Memory_region { IRQ_CONTROLLER_VT_CTRL_BASE, IRQ_CONTROLLER_VT_CTRL_SIZE },
            Memory_region { MCT_MMIO_BASE, MCT_MMIO_SIZE },
            Memory_region { UART_2_MMIO_BASE, UART_2_MMIO_SIZE }) { }


void Platform::enable_mmu()
{
	using Genode::Cpu;

	static volatile bool primary_cpu = true;
	pic.init_cpu_local();

	cpu.init(*reinterpret_cast<Genode::Translation_table*>(core_pd->table_base));

	Cpu::Sctlr::init();
	Cpu::Psr::write(Cpu::Psr::init_kernel());

	cpu.invalidate_inner_data_cache();

	/* primary cpu wakes up all others */
	if (primary_cpu && NR_OF_CPUS > 1) {
		primary_cpu = false;
		board.wake_up_all_cpus(&_start_setup_stack);
	}

	cpu.enable_mmu_and_caches((Genode::addr_t)core_pd->table_base);
}
