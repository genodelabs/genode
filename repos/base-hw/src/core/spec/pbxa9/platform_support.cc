/*
 * \brief   Parts of platform that are specific to PBXA9
 * \author  Martin Stein
 * \date    2012-04-27
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <cpu.h>
#include <pic.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


Memory_region_array & Platform::ram_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::RAM_0_BASE, Board::RAM_0_SIZE },
		Memory_region { Board::RAM_1_BASE, Board::RAM_1_SIZE });
}


Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		/* core timer and PIC */
		Memory_region { Board::CORTEX_A9_PRIVATE_MEM_BASE,
		                Board::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* core UART */
		Memory_region { Board::PL011_0_MMIO_BASE, Board::PL011_0_MMIO_SIZE },

		/* L2 Cache Controller */
		Memory_region { Board::PL310_MMIO_BASE, Board::PL310_MMIO_SIZE }
	);
}


Genode::Arm::User_context::User_context() { cpsr = Psr::init_user(); }
