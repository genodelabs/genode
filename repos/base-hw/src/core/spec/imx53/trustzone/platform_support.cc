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
#include <cpu.h>
#include <trustzone.h>
#include <csu.h>

using namespace Genode;

/* monitor exception vector address */
extern int _mon_kernel_entry;


void Kernel::init_trustzone(Pic * pic)
{
	using namespace Genode;

	/* check for compatibility */
	if (NR_OF_CPUS > 1) {
		PERR("trustzone not supported with multiprocessing");
		return;
	}
	/* set exception vector entry */
	Cpu::mon_exception_entry_at((Genode::addr_t)&_mon_kernel_entry);

	/* enable coprocessor 10 + 11 access for TZ VMs */
	Cpu::Nsacr::access_t v = 0;
	Cpu::Nsacr::Cpnsae10::set(v, 1);
	Cpu::Nsacr::Cpnsae11::set(v, 1);
	Cpu::Nsacr::write(v);

	/* configure non-secure interrupts */
	for (unsigned i = 0; i < Pic::NR_OF_IRQ; i++) {
		if ((i != Board::EPIT_1_IRQ) &&
			(i != Board::EPIT_2_IRQ) &&
			(i != Board::I2C_2_IRQ)  &&
			(i != Board::I2C_3_IRQ)  &&
			(i < Board::GPIO1_IRQL || i > Board::GPIO4_IRQH) &&
			(i < Board::GPIO5_IRQL || i > Board::GPIO7_IRQH))
			pic->unsecure(i);
	}

	/* configure central security unit */
	Genode::Csu csu(Board::CSU_BASE);
}


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Trustzone::SECURE_RAM_BASE, Trustzone::SECURE_RAM_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * mmio_regions(unsigned const i)
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
		{ Board::IRQ_CONTROLLER_BASE, Board::IRQ_CONTROLLER_SIZE },

		/* central security unit */
		{ Board::CSU_BASE, Board::CSU_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


bool Imx::Board::is_smp() { return false; }

Cpu::User_context::User_context() { cpsr = Psr::init_user_with_trustzone(); }
