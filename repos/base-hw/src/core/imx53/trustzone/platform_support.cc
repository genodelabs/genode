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

#include <drivers/trustzone.h>

/* core includes */
#include <platform.h>
#include <board.h>
#include <pic.h>
#include <processor_driver.h>
#include <trustzone.h>
#include <csu.h>

using namespace Genode;

/* monitor exception vector address */
extern int _mon_kernel_entry;


void Kernel::init_trustzone(Pic * pic)
{
	using namespace Genode;

	/* check for compatibility */
	if (PROCESSORS > 1) {
		PERR("trustzone not supported with multiprocessing");
		return;
	}
	/* set exception vector entry */
	Processor_driver::mon_exception_entry_at((Genode::addr_t)&_mon_kernel_entry);

	/* enable coprocessor access for TZ VMs */
	Processor_driver::allow_coprocessor_nonsecure();

	/* configure non-secure interrupts */
	for (unsigned i = 0; i < Pic::NR_OF_IRQ; i++) {
		if ((i != Imx53::Board::EPIT_1_IRQ) &&
			(i != Imx53::Board::EPIT_2_IRQ) &&
			(i != Imx53::Board::I2C_2_IRQ)  &&
			(i != Imx53::Board::I2C_3_IRQ)  &&
			(i < Imx53::Board::GPIO1_IRQL || i > Imx53::Board::GPIO4_IRQH) &&
			(i < Imx53::Board::GPIO5_IRQL || i > Imx53::Board::GPIO7_IRQH))
			pic->unsecure(i);
	}

	/* configure central security unit */
	Genode::Csu csu(Imx53::Board::CSU_BASE);
}


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Trustzone::SECURE_RAM_BASE, Trustzone::SECURE_RAM_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 0x07000000, 0x1000000  }, /* security controller */
		{ 0x10000000, 0x30000000 }, /* SATA, IPU, GPU      */
		{ 0x50000000, 0x20000000 }, /* Misc.               */
		{ Trustzone::NONSECURE_RAM_BASE, Trustzone::NONSECURE_RAM_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core UART */
		{ Board::UART_1_MMIO_BASE, Board::UART_1_MMIO_SIZE },

		/* core timer */
		{ Board::EPIT_1_MMIO_BASE, Board::EPIT_1_MMIO_SIZE },

		/* interrupt controller */
		{ Board::TZIC_MMIO_BASE, Board::TZIC_MMIO_SIZE },

		/* central security unit */
		{ Board::CSU_BASE, Board::CSU_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Processor_driver::User_context::User_context() { cpsr = Psr::init_user_with_trustzone(); }
