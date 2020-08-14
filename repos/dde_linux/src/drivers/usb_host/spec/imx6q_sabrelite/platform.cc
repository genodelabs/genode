/*
 * \brief  EHCI for Freescale i.MX6
 * \author Stefan Kalkowski
 * \date   2018-04-11
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <platform.h>
#include <lx_emul.h>

extern "C" int  module_ci_hdrc_platform_register();
extern "C" int  module_ci_hdrc_imx_driver_init();
extern "C" int  module_usbmisc_imx_driver_init();
extern "C" int  postcore_mxs_phy_module_init();

extern "C" void module_ehci_hcd_init();
extern "C" void module_xhci_hcd_init();

void platform_hcd_init(Genode::Env &, Services *services)
{
	module_ehci_hcd_init();
	module_ci_hdrc_platform_register();
	postcore_mxs_phy_module_init();
	module_usbmisc_imx_driver_init();
	module_ci_hdrc_imx_driver_init();

	lx_platform_device_init();
}

extern "C" int devm_extcon_register_notifier(struct device *dev, struct extcon_dev *edev, unsigned int id, struct notifier_block *nb)
{
	BUG();
	return -1;
}

extern "C" struct extcon_dev *extcon_get_edev_by_phandle(struct device *dev, int index)
{
	BUG();
	return NULL;
}

extern "C" int extcon_get_state(struct extcon_dev *edev, unsigned int id)
{
	BUG();
	return -1;
}
