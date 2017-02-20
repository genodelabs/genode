/*
 * \brief   Platform implementations specific for base-hw and Raspberry Pi
 * \author  Norman Feske
 * \author  Stefan Kalkowski
 * \date    2013-04-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

using Genode::Memory_region;

/**
 * Leave out the first page (being 0x0) from bootstraps RAM allocator,
 * some code does not feel happy with addresses being zero
 */
Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE + 0x1000,
                                    RAM_0_SIZE - 0x1000 }),
  late_ram_regions(Memory_region { RAM_0_BASE, 0x1000 }),
  core_mmio(Memory_region { PL011_0_MMIO_BASE,
                            PL011_0_MMIO_SIZE },
            Memory_region { SYSTEM_TIMER_MMIO_BASE,
                            SYSTEM_TIMER_MMIO_SIZE },
            Memory_region { IRQ_CONTROLLER_BASE,
                            IRQ_CONTROLLER_SIZE },
            Memory_region { USB_DWC_OTG_BASE,
                            USB_DWC_OTG_SIZE }) {}


void Platform::enable_mmu()
{
	Genode::Cpu::Sctlr::init();
	Genode::Cpu::Psr::write(Genode::Cpu::Psr::init_kernel());

	/* check for mapping restrictions */
	assert(!Genode::Cpu::restricted_page_mappings());

	cpu.enable_mmu_and_caches((addr_t)core_pd->table_base);
}
