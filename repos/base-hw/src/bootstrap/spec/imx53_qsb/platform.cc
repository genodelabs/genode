/*
 * \brief   Specific i.MX53 bootstrap implementations
 * \author  Stefan Kalkowski
 * \date    2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <spec/arm/imx_aipstz.h>

using namespace Board;

bool Board::secure_irq(unsigned) { return true; }


Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM0_BASE, RAM0_SIZE },
                    Memory_region { RAM1_BASE, RAM1_SIZE }),
  core_mmio(Memory_region { UART_1_MMIO_BASE, UART_1_MMIO_SIZE },
            Memory_region { EPIT_1_MMIO_BASE, EPIT_1_MMIO_SIZE },
            Memory_region { IRQ_CONTROLLER_BASE, IRQ_CONTROLLER_SIZE })
{
	Aipstz aipstz_1(AIPS_1_MMIO_BASE);
	Aipstz aipstz_2(AIPS_2_MMIO_BASE);
}
