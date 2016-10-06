/*
 * \brief  Specific core implementations
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2014-02-25
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <cpu.h>

#include <base/internal/unmanaged_singleton.h>

using namespace Genode;

Memory_region_array & Platform::ram_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::RAM0_BASE, Board::RAM0_SIZE });
}


Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::UART_1_MMIO_BASE,                     /* UART */
		                Board::UART_1_MMIO_SIZE },
		Memory_region { Board::CORTEX_A9_PRIVATE_MEM_BASE, /* irq controller */
		                Board::CORTEX_A9_PRIVATE_MEM_SIZE },    /* and timer */
		Memory_region { Board::PL310_MMIO_BASE,       /* l2 cache controller */
		                Board::PL310_MMIO_SIZE });
}


Genode::Arm::User_context::User_context() { cpsr = Psr::init_user(); }


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
