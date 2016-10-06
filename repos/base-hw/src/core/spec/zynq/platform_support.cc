/*
 * \brief   Platform implementations specific for base-hw and Zynq
 * \author  Johannes Schlatow
 * \author  Stefan Kalkowski
 * \date    2014-12-15
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
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
		Memory_region { Board::RAM_0_BASE, Board::RAM_0_SIZE });
}


Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::CORTEX_A9_PRIVATE_MEM_BASE, /* timer and PIC */
		                Board::CORTEX_A9_PRIVATE_MEM_SIZE },
		Memory_region { Board::KERNEL_UART_BASE,                    /* UART */
		                Board::KERNEL_UART_SIZE },
		Memory_region { Board::PL310_MMIO_BASE,      /* L2 cache controller */
		                Board::PL310_MMIO_SIZE });
}


Genode::Arm::User_context::User_context() { cpsr = Psr::init_user(); }


bool Cortex_a9::Board::errata(Cortex_a9::Board::Errata err) {
	return false; }
