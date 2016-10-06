/*
 * \brief   Platform implementations specific for base-hw and Raspberry Pi
 * \author  Norman Feske
 * \date    2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
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
		Memory_region { Board::RAM_0_BASE, Board::RAM_0_SIZE });
}


Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::PL011_0_MMIO_BASE,       /* UART */
		                Board::PL011_0_MMIO_SIZE },
		Memory_region { Board::SYSTEM_TIMER_MMIO_BASE,  /* system timer */
		                Board::SYSTEM_TIMER_MMIO_SIZE },
		Memory_region { Board::IRQ_CONTROLLER_BASE,     /* IRQ controller */
		                Board::IRQ_CONTROLLER_SIZE },
		Memory_region { Board::USB_DWC_OTG_BASE,        /* DWC OTG USB */
		                Board::USB_DWC_OTG_SIZE }       /* controller  */
	);
}


Cpu::User_context::User_context() { cpsr = Psr::init_user(); }
