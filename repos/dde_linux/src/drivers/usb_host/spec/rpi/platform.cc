/*
 * \brief  USB initialization for Raspberry Pi
 * \author Norman Feske
 * \date   2013-09-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <io_mem_session/connection.h>
#include <util/mmio.h>
#include <platform_session/connection.h>


/* emulation */
#include <platform.h>
#include <lx_emul.h>
#include <timer_session/connection.h>

/* dwc-otg */
#define new new_
#include <dwc_otg_dbg.h>
#undef new

using namespace Genode;

unsigned dwc_irq();


/************************************************
 ** Resource info passed to the dwc_otg driver **
 ************************************************/

enum {
	DWC_BASE = 0x20980000,
	DWC_SIZE = 0x20000,
};


static resource _dwc_otg_resource[] =
{
	{ DWC_BASE, DWC_BASE + DWC_SIZE - 1, "dwc_otg", IORESOURCE_MEM },
	{ dwc_irq(), dwc_irq(), "dwc_otg-irq" /* name unused */, IORESOURCE_IRQ }
};


/*******************
 ** Init function **
 *******************/

extern "C" void module_dwc_otg_driver_init();
extern bool fiq_enable, fiq_fsm_enable;

void platform_hcd_init(Services *services)
{
	/* enable USB power */
	Platform::Connection platform(services->env);
	platform.power_state(Platform::Session::POWER_USB_HCD, true);

	/* disable fiq optimization */
	fiq_enable = false;
	fiq_fsm_enable = false;

	module_dwc_otg_driver_init();

	/* setup host-controller platform device */
	platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
	pdev->name            = (char *)"dwc_otg";
	pdev->id              = 0;
	pdev->num_resources   = sizeof(_dwc_otg_resource)/sizeof(resource);
	pdev->resource        = _dwc_otg_resource;

	/* needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c' */
	static u64 dma_mask         = ~(u64)0;
	pdev->dev.dma_mask          = &dma_mask;
	pdev->dev.coherent_dma_mask = ~0;

	platform_device_register(pdev);
}
