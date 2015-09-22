/**
 * \brief  X86 platform initialization
 * \author Sebastian Sumpf
 * \date   2013-05-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <platform.h>
#include <platform/platform.h>


extern "C" void module_ax88179_178a_driver_init();
extern "C" void module_usbnet_init();
extern "C" void module_ehci_hcd_init();
extern "C" void module_ehci_pci_init();
extern "C" void module_uhci_hcd_init();
extern "C" void module_xhci_hcd_init();

void platform_hcd_init(Services *s)
{
	if (s->nic) {
		module_usbnet_init();
		module_ax88179_178a_driver_init();
	}

	if (s->xhci)
		module_xhci_hcd_init();

	/* ehci_hcd should always be loaded before uhci_hcd and ohci_hcd, not after */
	if (s->ehci) {
		module_ehci_hcd_init();
		module_ehci_pci_init();
	}

	if (s->uhci)
		module_uhci_hcd_init();
}




