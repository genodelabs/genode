/*
 * \brief   Specific core implementations
 * \author  Stefan Kalkowski
 * \date    2012-10-24
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

#include <base/internal/unmanaged_singleton.h>

using namespace Genode;

Memory_region_array & Platform::ram_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::RAM0_BASE, Board::RAM0_SIZE },
		Memory_region { Board::RAM1_BASE, Board::RAM1_SIZE });
}


Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::UART_1_MMIO_BASE,       /* UART */
		                Board::UART_1_MMIO_SIZE },
		Memory_region { Board::EPIT_1_MMIO_BASE,       /* timer */
		                Board::EPIT_1_MMIO_SIZE },
		Memory_region { Board::IRQ_CONTROLLER_BASE,    /* irq controller */
		                Board::IRQ_CONTROLLER_SIZE });
}


Cpu::User_context::User_context() { cpsr = Psr::init_user(); }
