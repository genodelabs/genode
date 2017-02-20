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


Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE + 0x1000,
                                    RAM_0_SIZE - 0x1000 }),
  late_ram_regions(Memory_region { RAM_0_BASE, 0x1000 }),
  core_mmio(Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
                            CORTEX_A9_PRIVATE_MEM_SIZE },
            Memory_region { KERNEL_UART_BASE,
                            KERNEL_UART_SIZE },
            Memory_region { PL310_MMIO_BASE,
                            PL310_MMIO_SIZE }) { }


bool Cortex_a9::Board::errata(Cortex_a9::Board::Errata err) {
	return false; }
