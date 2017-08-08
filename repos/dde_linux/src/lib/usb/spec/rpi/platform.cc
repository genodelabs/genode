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


/***************************************
 ** Supplement to lx_emul environment **
 ***************************************/

#if VERBOSE_LX_EMUL
#define TRACE lx_printf("\033[32m%s\033[0m called, not implemented\n", __PRETTY_FUNCTION__)
#else
#define TRACE
#endif

#define DUMMY(retval, name) \
extern "C" long name() { \
	lx_printf("\033[32m%s\033[0m called, not implemented, stop\n", #name); \
	bt(); \
	for (;;); \
	return retval; \
}

#define CHECKED_DUMMY(retval, name) \
extern "C" long name() { \
	lx_printf("\033[32m%s\033[0m called, not implemented, ignored\n", #name); \
	bt(); \
	return retval; \
}

#define SILENT_DUMMY(retval, name) \
extern "C" long name() { return retval; }


/*********************
 ** linux/hardirq.h **
 *********************/

int in_irq()
{
	TRACE;
	return 0;
}


/*******************
 ** linux/delay.h **
 *******************/

unsigned long loops_per_jiffy = 1;

/***********************************
 ** Dummies for unused PCD driver **
 ***********************************/

/*
 * The PCD driver is used for driving the DWC-OTG device as gadget. The
 * Raspberry Pi solely supports the use of the controller as host device.
 * Hence, the PCD parts are not needed.
 */

DUMMY(-1, dwc_otg_pcd_disconnect_us);
DUMMY(-1, dwc_otg_pcd_remote_wakeup);
DUMMY(-1, dwc_otg_pcd_get_rmwkup_enable);
DUMMY(-1, dwc_otg_pcd_initiate_srp);
DUMMY(-1, pcd_remove);
SILENT_DUMMY( 0, pcd_init);


/************************************************************************
 ** Prevent use of FIQ fix, need to resolve FIQ-related symbols anyway **
 ************************************************************************/

void local_fiq_disable() { }
void local_fiq_enable() { }
extern "C" void fiq_fsm_spin_lock(void *lock) { }
extern "C" void fiq_fsm_spin_unlock(void *lock) { }
int claim_fiq(struct fiq_handler *f) { return 0; }
void set_fiq_regs(struct pt_regs const *regs) { }
void set_fiq_handler(void *start, unsigned int length) { }
void enable_fiq() { }

void __FIQ_Branch(unsigned long *regs) { TRACE; }

extern "C" int fiq_fsm_too_late(struct fiq_state *st, int n) { TRACE; return 0; }
extern "C" void dwc_otg_fiq_nop(struct fiq_state *state) { TRACE; }
extern "C" void dwc_otg_fiq_fsm(struct fiq_state *state, int num_channels) { TRACE; }

unsigned char _dwc_otg_fiq_stub, _dwc_otg_fiq_stub_end;


/***********************
 ** asm/dma_mapping.h **
 ***********************/

void *dma_to_virt(struct device *dev, dma_addr_t phys)
{
	return phys_to_virt(phys);
}


/*******************
 ** linux/timer.h **
 *******************/

struct tvec_base { };
struct tvec_base boot_tvec_bases;


/*******************
 ** Init function **
 *******************/

extern "C" void module_dwc_otg_driver_init();
extern "C" int  module_usbnet_init();
extern "C" int  module_smsc95xx_driver_init();

void platform_hcd_init(Services *services)
{
	/* enable USB power */
	Platform::Connection platform;
	platform.power_state(Platform::Session::POWER_USB_HCD, true);

	/* register network */
	if (services->nic) {
		module_usbnet_init();
		module_smsc95xx_driver_init();
	}

	/* disable fiq optimization */
	fiq_enable = false;
	fiq_fsm_enable = false;

	bool const verbose = false;
	if (verbose)
		g_dbg_lvl = DBG_HCD | DBG_CIL | DBG_HCD_URB;

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
