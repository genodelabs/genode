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
#include <processor_driver.h>

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
	Processor_driver::mon_exception_entry_at((Genode::addr_t)&_mon_kernel_entry);

	/* enable coprocessor access for TZ VMs */
	Processor_driver::allow_coprocessor_nonsecure();

	/* set unsecure IRQs */
	pic->unsecure(34); //Timer 0/1
	pic->unsecure(35); //Timer 2/3
	pic->unsecure(36); //RTC
	pic->unsecure(37); //UART0
	pic->unsecure(41); //MCI0
	pic->unsecure(42); //MCI1
	pic->unsecure(43); //AACI
	pic->unsecure(44); //KMI0
	pic->unsecure(45); //KMI1
	pic->unsecure(47); //ETHERNET
	pic->unsecure(48); //USB
}
