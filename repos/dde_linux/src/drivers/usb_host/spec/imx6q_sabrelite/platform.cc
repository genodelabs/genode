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

	/* USB PHY initialization */
	{
		static resource usbphy_res[] =
		{
			{ 0x020ca000, 0x020cafff, "usbphy", IORESOURCE_MEM },
			{ 77, 77, "usbphy-irq", IORESOURCE_IRQ },
		};

		platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
		pdev->name = (char *)"mxs_phy";
		pdev->id   = 0;
		pdev->num_resources = 2;
		pdev->resource = usbphy_res;

		pdev->dev.of_node             = (device_node*)kzalloc(sizeof(device_node), 0);
		pdev->dev.of_node->properties = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->name  = "compatible";
		pdev->dev.of_node->properties->value = (void*)"fsl,imx6q-usbphy";
		pdev->dev.of_node->properties->next = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->next->name  = "fsl,anatop";
		pdev->dev.of_node->properties->next->value = (void*)0xdeaddead;

		platform_device_register(pdev);
	}

	device_node * usbmisc_of_node = nullptr;

	/* USB MISC initialization */
	{
		static resource usbmisc_res[] = { { 0x02184800, 0x021849ff, "usbmisc", IORESOURCE_MEM }, };

		platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
		pdev->name = (char *)"usbmisc_imx";
		pdev->id   = 1;
		pdev->num_resources = 1;
		pdev->resource = usbmisc_res;

		pdev->dev.of_node             = (device_node*)kzalloc(sizeof(device_node), 0);
		pdev->dev.of_node->properties = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->name  = "compatible";
		pdev->dev.of_node->properties->value = (void*)"fsl,imx6q-usbmisc";
		pdev->dev.of_node->dev               = &pdev->dev;
		usbmisc_of_node = pdev->dev.of_node;

		platform_device_register(pdev);
	}

	/* setup EHCI-controller platform device */
	{
		static resource ehci_res[] =
		{
			{ 0x02184200, 0x021843ff, "imx_usb", IORESOURCE_MEM },
			{ 72, 72, "imx_usb-irq", IORESOURCE_IRQ },
		};

		platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
		pdev->name = (char *)"imx_usb";
		pdev->id   = 2;
		pdev->num_resources = 2;
		pdev->resource = ehci_res;

		pdev->dev.of_node             = (device_node*)kzalloc(sizeof(device_node), 0);
		pdev->dev.of_node->properties = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->name  = "compatible";
		pdev->dev.of_node->properties->value = (void*)"fsl,imx6q-usb";
		pdev->dev.of_node->properties->next = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->next->name  = "fsl,usbmisc";
		pdev->dev.of_node->properties->next->value = (void*)usbmisc_of_node;
		pdev->dev.of_node->properties->next->next = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->next->next->name  = "dr_mode";
		pdev->dev.of_node->properties->next->next->value = (void*)"host";

		/*
		 * Needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c'
		 */
		static u64 dma_mask = ~(u64)0;
		pdev->dev.dma_mask = &dma_mask;
		pdev->dev.coherent_dma_mask = ~0;

		platform_device_register(pdev);
	}
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
