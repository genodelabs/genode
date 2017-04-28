/*
 * \brief   Platform implementations specific for base-hw and Zynq
 * \author  Johannes Schlatow
 * \author  Stefan Kalkowski
 * \date    2014-12-15
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE + 0x1000,
                                    RAM_0_SIZE - 0x1000 }),
  late_ram_regions(Memory_region { RAM_0_BASE, 0x1000 }),
  core_mmio(Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
                            CORTEX_A9_PRIVATE_MEM_SIZE },
            Memory_region { UART_0_MMIO_BASE,
                            UART_SIZE },
            Memory_region { PL310_MMIO_BASE,
                            PL310_MMIO_SIZE }) { }


bool Bootstrap::Cpu::errata(Bootstrap::Cpu::Errata err) {
	return false; }
