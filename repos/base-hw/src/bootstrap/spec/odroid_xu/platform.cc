/*
 * \brief   Parts of platform that are specific to Odroid XU
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

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE }),
  core_mmio(Memory_region { IRQ_CONTROLLER_BASE, IRQ_CONTROLLER_SIZE },
            Memory_region { IRQ_CONTROLLER_VT_CTRL_BASE, IRQ_CONTROLLER_VT_CTRL_SIZE },
            Memory_region { MCT_MMIO_BASE, MCT_MMIO_SIZE },
            Memory_region { UART_2_MMIO_BASE, UART_2_MMIO_SIZE }) { }


void Bootstrap::Platform::enable_mmu()
{
	pic.init_cpu_local();
	Cpu::Sctlr::init();
	Cpu::Cpsr::init();
	cpu.invalidate_data_cache();
	cpu.enable_mmu_and_caches((Genode::addr_t)core_pd->table_base);
}
