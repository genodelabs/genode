/*
 * \brief   Specific core implementations
 * \author  Stefan Kalkowski
 * \date    2017-01-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <drivers/trustzone.h>

using Genode::Memory_region;

Platform::Board::Board()
: early_ram_regions(Memory_region { Trustzone::SECURE_RAM_BASE,
                                    Trustzone::SECURE_RAM_SIZE }),
  core_mmio(Memory_region { UART_1_MMIO_BASE, UART_1_MMIO_SIZE },
            Memory_region { EPIT_1_MMIO_BASE, EPIT_1_MMIO_SIZE },
            Memory_region { IRQ_CONTROLLER_BASE, IRQ_CONTROLLER_SIZE },
            Memory_region { CSU_BASE, CSU_SIZE }) {
	init(); }
