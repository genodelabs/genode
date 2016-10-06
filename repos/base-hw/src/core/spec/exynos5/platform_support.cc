/*
 * \brief   Parts of platform that are specific to Arndale
 * \author  Martin Stein
 * \date    2012-04-27
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <board.h>
#include <platform.h>

#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


Memory_region_array & Platform::ram_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::RAM_0_BASE, Board::RAM_0_SIZE } );
}


Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::IRQ_CONTROLLER_BASE, Board::IRQ_CONTROLLER_SIZE },
		Memory_region { Board::MCT_MMIO_BASE, Board::MCT_MMIO_SIZE },
		Memory_region { Board::UART_2_MMIO_BASE, 0x1000 });
}
