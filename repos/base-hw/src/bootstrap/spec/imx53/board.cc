/*
 * \brief   Specific bootstrap implementations
 * \author  Stefan Kalkowski
 * \date    2017-01-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <board.h>

using Genode::Memory_region;

Platform::Board::Board()
: early_ram_regions(Memory_region { RAM0_BASE, RAM0_SIZE },
                    Memory_region { RAM1_BASE, RAM1_SIZE }),
  core_mmio(Memory_region { UART_1_MMIO_BASE, UART_1_MMIO_SIZE },
            Memory_region { EPIT_1_MMIO_BASE, EPIT_1_MMIO_SIZE },
            Memory_region { IRQ_CONTROLLER_BASE, IRQ_CONTROLLER_SIZE }) {
	init(); }
