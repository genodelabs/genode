/*
 * \brief  XHCI for Freescale i.MX8
 * \author Alexander Boettcher
 * \date   2019-12-02
 *
 * The driver is supposed to work solely if in the bootloader (uboot) the
 * usb controller got powered on and the bootloader does not disable it on
 * Genode boot.
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <platform.h>
#include <lx_emul.h>

extern "C" void module_dwc3_driver_init();
extern "C" void module_xhci_plat_init();

void platform_hcd_init(Genode::Env &, Services *)
{
	module_dwc3_driver_init();
	module_xhci_plat_init();
	lx_platform_device_init();
}
