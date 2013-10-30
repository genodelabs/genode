/*
 * \brief   Platform implementations specific for base-hw and Panda A2
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
#include <kernel/irq.h>

using namespace Genode;

namespace Kernel { void init_platform(); }

/**
 * Interrupts that core shall provide to users
 */
static unsigned irq_ids[] =
{
	Board::GP_TIMER_3_IRQ,
	Board::TL16C750_1_IRQ,
	Board::TL16C750_2_IRQ,
	Board::TL16C750_4_IRQ,
	Board::GPIO1_IRQ,
	Board::GPIO2_IRQ,
	Board::GPIO3_IRQ,
	Board::GPIO4_IRQ,
	Board::GPIO5_IRQ,
	Board::GPIO6_IRQ
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
		{ Board::RAM_0_BASE, Board::RAM_0_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_0_BASE, Board::MMIO_0_SIZE },
		{ Board::MMIO_1_BASE, Board::MMIO_1_SIZE },
		{ Board::DSS_MMIO_BASE, Board::DSS_MMIO_SIZE },
		{ Board::DISPC_MMIO_BASE, Board::DISPC_MMIO_SIZE },
		{ Board::HDMI_MMIO_BASE, Board::HDMI_MMIO_SIZE }
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
		{ Board::TL16C750_3_MMIO_BASE, Board::TL16C750_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}
