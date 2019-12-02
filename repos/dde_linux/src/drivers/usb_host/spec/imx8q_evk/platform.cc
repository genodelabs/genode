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

void platform_hcd_init(Genode::Env &, Services *services)
{
	module_dwc3_driver_init();
	module_xhci_plat_init();

	/* setup XHCI-controller platform device */
	{
		static resource xhci_res[] =
		{
			{ 0x38200000ul, 0x38200000ul + 0x10000 - 1, "dwc3", IORESOURCE_MEM },
			{ 41 + 32, 41 + 32, "dwc3-irq", IORESOURCE_IRQ },
		};

		platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
		pdev->name = (char *)"dwc3";
		pdev->id   = 2;
		pdev->num_resources = 2;
		pdev->resource = xhci_res;

		pdev->dev.of_node             = (device_node*)kzalloc(sizeof(device_node), 0);
		pdev->dev.of_node->properties = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->name  = "compatible";
		pdev->dev.of_node->properties->value = (void*)"fsl,imx8mq-dwc3";
		pdev->dev.of_node->properties->next = (property*)kzalloc(sizeof(property), 0);
		pdev->dev.of_node->properties->next->name  = "dr_mode";
		pdev->dev.of_node->properties->next->value = (void*)"host";

		/*
		 * Needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c'
		 */
		static u64 dma_mask = ~(u64)0;
		pdev->dev.dma_mask = &dma_mask;
		pdev->dev.coherent_dma_mask = ~0;

		platform_device_register(pdev);
	}
}
