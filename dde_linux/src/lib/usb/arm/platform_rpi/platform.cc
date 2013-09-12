/*
 * \brief  USB initialization for Raspberry Pi
 * \author Norman Feske
 * \date   2013-09-11
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <io_mem_session/connection.h>
#include <util/mmio.h>

/* emulation */
#include <platform/platform.h>
#include <lx_emul.h>

/* dwc-otg */
#define new new_
#include <dwc_otg_dbg.h>
#undef new

using namespace Genode;


/************************************************
 ** Resource info passed to the dwc_otg driver **
 ************************************************/

enum {
	DWC_BASE = 0x20980000,
	DWC_SIZE = 0x20000,

	DWC_IRQ  = 17,
};


static resource _dwc_otg_resource[] =
{
	{ DWC_BASE, DWC_BASE + DWC_SIZE - 1, "dwc_otg", IORESOURCE_MEM },
	{ DWC_IRQ, DWC_IRQ, "dwc_otg-irq" /* name unused */, IORESOURCE_IRQ }
};


/***************************************
 ** Supplement to lx_emul environment **
 ***************************************/

#if VERBOSE_LX_EMUL
#define TRACE dde_kit_printf("\033[32m%s\033[0m called, not implemented\n", __PRETTY_FUNCTION__)
#else
#define TRACE
#endif

#define DUMMY(retval, name) \
extern "C" long name() { \
	dde_kit_printf("\033[32m%s\033[0m called, not implemented, stop\n", #name); \
	bt(); \
	for (;;); \
	return retval; \
}

#define CHECKED_DUMMY(retval, name) \
extern "C" long name() { \
	dde_kit_printf("\033[32m%s\033[0m called, not implemented, ignored\n", #name); \
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
	TRACE; PDBG("called by %p", __builtin_return_address(0));
	return 0;
}


/*******************
 ** linux/delay.h **
 *******************/

unsigned long loops_per_jiffy = 1;


/*********************
 ** linux/jiffies.h **
 *********************/

unsigned int jiffies_to_msecs(const unsigned long j)
{
	PDBG("not implemented. stop");
	return 1;
}


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
DUMMY(-1, printk_once);


/************************************************************************
 ** Prevent use of FIQ fix, need to resolve FIQ-related symbols anyway **
 ************************************************************************/

int fiq_fix_enable = false;

extern int fiq_split_enable;

void local_fiq_disable() { }
void local_fiq_enable() { }
int claim_fiq(struct fiq_handler *f) { return 0; }
void set_fiq_regs(struct pt_regs const *regs) { }
void set_fiq_handler(void *start, unsigned int length) { }
void enable_fiq() { }

void __FIQ_Branch(unsigned long *regs) { TRACE; }


/***********************
 ** linux/workqueue.h **
 ***********************/

struct workqueue_struct *create_singlethread_workqueue(char *)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	return wq;
}

void destroy_workqueue(struct workqueue_struct *wq) { TRACE; }


bool queue_work(struct workqueue_struct *wq, struct work_struct *work) { TRACE; return 0; }


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

struct Services;

void platform_hcd_init(Services *)
{
	/* disable split-enable fix, otherwise, fiq_fix will be implied */
	fiq_split_enable = false;

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
