/*
 * \brief   Platform implementations specific for base-hw and Zynq
 * \author  Johannes Schlatow
 * \date    2014-12-15
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <cpu.h>
#include <pic.h>
#include <unmanaged_singleton.h>

using namespace Genode;

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::RAM_0_BASE, Board::RAM_0_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_0_BASE, Board::MMIO_0_SIZE },
		{ Board::MMIO_1_BASE, Board::MMIO_1_SIZE },
		{ Board::QSPI_MMIO_BASE, Board::QSPI_MMIO_SIZE },
		{ Board::OCM_MMIO_BASE,  Board::OCM_MMIO_SIZE },
		{ Board::AXI_0_MMIO_BASE, Board::AXI_0_MMIO_SIZE },
		{ Board::AXI_1_MMIO_BASE, Board::AXI_1_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core timer and PIC */
		{ Board::CORTEX_A9_PRIVATE_MEM_BASE,
		  Board::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* core UART */
		{ Board::UART_0_MMIO_BASE, Board::UART_SIZE },

		/* L2 cache controller */
		{ Board::PL310_MMIO_BASE, Board::PL310_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Cpu::User_context::User_context() { cpsr = Psr::init_user(); }


static Genode::Pl310 * l2_cache() {
	return unmanaged_singleton<Genode::Pl310>(Board::PL310_MMIO_BASE); }


void Genode::Board::outer_cache_invalidate() { l2_cache()->invalidate(); }
void Genode::Board::outer_cache_flush()      { l2_cache()->flush();      }
void Genode::Board::prepare_kernel()         { l2_cache()->invalidate(); }
