/*
 * \brief  Specific bootstrap implementations
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2014-02-25
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <cpu.h>

using namespace Genode;

Platform::Board::Board()
: early_ram_regions(Memory_region { RAM0_BASE, RAM0_SIZE }),
  core_mmio(Memory_region { UART_1_MMIO_BASE,
                            UART_1_MMIO_SIZE },
            Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
                            CORTEX_A9_PRIVATE_MEM_SIZE },
            Memory_region { PL310_MMIO_BASE,
                            PL310_MMIO_SIZE }) { init(); }


bool Cortex_a9::Board::errata(Cortex_a9::Board::Errata err)
{
	switch (err) {
		case Cortex_a9::Board::ARM_754322:
		case Cortex_a9::Board::ARM_764369:
		case Cortex_a9::Board::ARM_775420:
		case Cortex_a9::Board::PL310_588369:
		case Cortex_a9::Board::PL310_727915:
		case Cortex_a9::Board::PL310_769419:
			return true;
	};
	return false;
}
