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
#include <processor_driver.h>
#include <pic.h>
#include <kernel/irq.h>

using namespace Genode;

namespace Kernel { void init_platform(); }

/**
 * Interrupts that core shall provide to users
 */
static unsigned irq_ids[] =
{
	Board::SP804_0_1_IRQ,
	Board::KMI_0_IRQ,
	Board::KMI_1_IRQ,
	Board::ETHERNET_IRQ,
	Board::PL011_1_IRQ,
	Board::PL011_2_IRQ,
	Board::PL011_3_IRQ,
	Board::PL180_IRQ_0,
	Board::PL180_IRQ_1
};

enum { IRQ_IDS_SIZE = sizeof(irq_ids)/sizeof(irq_ids[0]) };


void Kernel::init_platform()
{
	/* make user IRQs become known by cores IRQ session backend and kernel */
	static uint8_t _irqs[IRQ_IDS_SIZE][sizeof(Irq)];
	for (unsigned i = 0; i < IRQ_IDS_SIZE; i++) {
		new (_irqs[i]) Irq(irq_ids[i]);
	}
}


unsigned * Platform::_irq(unsigned const i)
{
	return i < IRQ_IDS_SIZE ? &irq_ids[i] : 0;
}


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::RAM_0_BASE, Board::RAM_0_SIZE },
		{ Board::RAM_1_BASE, Board::RAM_1_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_0_BASE, Board::MMIO_0_SIZE },
		{ Board::MMIO_1_BASE, Board::MMIO_1_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core timer and PIC */
		{ Board::CORTEX_A9_PRIVATE_MEM_BASE, Board::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* core UART */
		{ Board::PL011_0_MMIO_BASE, Board::PL011_0_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Cpu::User_context::User_context() { cpsr = Psr::init_user(); }
