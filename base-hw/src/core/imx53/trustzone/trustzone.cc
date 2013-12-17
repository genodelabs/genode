/*
 * \brief  TrustZone specific functions for Versatile Express
 * \author Stefan Kalkowski
 * \date   2012-10-10
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <trustzone.h>
#include <pic.h>
#include <cpu.h>
#include <csu.h>
#include <board.h>

/* monitor exception vector address */
extern int _mon_kernel_entry;


void Kernel::init_trustzone(Pic * pic)
{
	/* check for compatibility */
	if (PROCESSORS > 1) {
		PERR("trustzone not supported with multiprocessing");
		return;
	}
	/* set exception vector entry */
	Genode::Cpu::mon_exception_entry_at((Genode::addr_t)&_mon_kernel_entry);

	/* enable coprocessor access for TZ VMs */
	Genode::Cpu::allow_coprocessor_nonsecure();

	/* configure non-secure interrupts */
	for (unsigned i = 0; i <= Pic::MAX_INTERRUPT_ID; i++) {
		if ((i != Imx53::Board::EPIT_1_IRQ) &&
			(i != Imx53::Board::EPIT_2_IRQ) &&
			(i != Imx53::Board::I2C_2_IRQ)  &&
			(i != Imx53::Board::I2C_3_IRQ)  &&
			(i < Imx53::Board::GPIO1_IRQL || i > Imx53::Board::GPIO4_IRQH) &&
			(i < Imx53::Board::GPIO5_IRQL || i > Imx53::Board::GPIO7_IRQH))
			pic->unsecure(i);
	}

	/* configure central security unit */
	Genode::Csu csu(0x63f9c000);
}
