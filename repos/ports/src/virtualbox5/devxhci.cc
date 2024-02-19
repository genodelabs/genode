/*
 * \brief  NEC XHCI device frontend
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2015-12-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <libc/allocator.h>
#include <util/list.h>

/* qemu-usb includes */
#include <qemu/usb.h>

/* libc internal includes */
#include <internal/thread_create.h>

/* Virtualbox includes */
#define LOG_GROUP LOG_GROUP_DEV_EHCI
#include <VBox/pci.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/asm-math.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#ifdef IN_RING3
# include <iprt/alloca.h>
# include <iprt/mem.h>
# include <iprt/thread.h>
# include <iprt/uuid.h>
#endif
#include <VBox/vusb.h>
#include <VBoxDD.h>

/* VBox Genode specific */
#include "vmm.h"

static bool const verbose_timer = false;


/************************
 ** xHCI device struct **
 ************************/

struct Timer_queue;


struct XHCI
{
	/** The PCI device. */
	PDMPCIDEV    PciDev;

	/** Pointer to the device instance - R3 ptr. */
	PPDMDEVINSR3 pDevInsR3;

	/** Pointer to the device instance - R0 ptr */
	PPDMDEVINSR0 pDevInsR0;

	/** Pointer to the device instance - RC ptr. */
	PPDMDEVINSRC pDevInsRC;

	/** Address of the MMIO region assigned by PCI. */
	RTGCPHYS32   MMIOBase;

	PTMTIMERR3        controller_timer;
	Timer_queue      *timer_queue;
	Qemu::Controller *ctl;

	Genode::Entrypoint *usb_ep;
};


/** Pointer to XHCI device data. */
typedef struct XHCI *PXHCI;
/** Read-only pointer to the XHCI device data. */
typedef struct XHCI const *PCXHCI;


/*************************************
 ** Qemu::Controller helper classes **
 *************************************/

struct Timer_queue : public Qemu::Timer_queue
{
	struct Context : public Genode::List<Context>::Element
	{
		uint64_t timeout_abs_ns = ~0ULL;
		bool     pending        = false;

		void  *qtimer     = nullptr;
		void (*cb)(void*) = nullptr;
		void  *data       = nullptr;

		Context(void *qtimer, void (*cb)(void*), void *data)
		: qtimer(qtimer), cb(cb), data(data) { }
	};

	Genode::List<Context> _context_list;
	PTMTIMER              tm_timer;

	void _append_new_context(void *qtimer, void (*cb)(void*), void *data)
	{
		Context *new_ctx = new (vmm_heap()) Context(qtimer, cb, data);
		_context_list.insert(new_ctx);
	}

	Context *_find_context(void const *qtimer)
	{
		for (Context *c = _context_list.first(); c; c = c->next())
			if (c->qtimer == qtimer)
				return c;
		return nullptr;
	}

	Context *_min_pending()
	{
		Context *min = nullptr;
		for (min = _context_list.first(); min; min = min->next())
			if (min && min->pending)
				break;

		if (!min || !min->next())
			return min;

		for (Context *c = min->next(); c; c = c->next()) {
			if ((c->timeout_abs_ns < min->timeout_abs_ns) && c->pending)
				min = c;
		}

		return min;
	}

	void _program_min_timer()
	{
		Context *min = _min_pending();
		if (min == nullptr) return;

		if (TMTimerIsActive(tm_timer))
			TMTimerStop(tm_timer);

		uint64_t const now = TMTimerGetNano(tm_timer);
		if (min->timeout_abs_ns < now)
			TMTimerSetNano(tm_timer, 0);
		else
			TMTimerSetNano(tm_timer, min->timeout_abs_ns - now);
	}

	void _deactivate_timer(void *qtimer)
	{
		Context *c = _find_context(qtimer);
		if (c == nullptr) {
			Genode::error("qtimer: ", qtimer, " not found");
			throw -1;
		}

		if (c == _min_pending()) {
			c->pending = false;
			TMTimerStop(tm_timer);
			_program_min_timer();
		}

		c->pending = false;
	}

	Timer_queue(PTMTIMER timer) : tm_timer(timer) { }

	void timeout()
	{
		uint64_t now = TMTimerGetNano(tm_timer);

		for (Context *c = _context_list.first(); c; c = c->next()) {
			if (c->pending && c->timeout_abs_ns <= now) {
				c->pending = false;
				Qemu::usb_timer_callback(c->cb, c->data);
			}
		}

		_program_min_timer();
	}

	/**********************
	 ** TMTimer callback **
	 **********************/

	static DECLCALLBACK(void) tm_timer_cb(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
	{
		PXHCI pThis    = PDMINS_2_DATA(pDevIns, PXHCI);
		Timer_queue *q = pThis->timer_queue;

		q->timeout();
	}


	unsigned count_timer()
	{
		unsigned res = 0;

		for (Context *c = _context_list.first(); c; c = c->next()) {
			if (c->pending) Genode::log("timer: ", c, " is pending");
			res++;
		}

		return res;
	}

	/*********************************
	 ** Qemu::Timer_queue interface **
	 *********************************/

	Qemu::int64_t get_ns() { return TMTimerGetNano(tm_timer); }

	Genode::Mutex _timer_mutex { };

	void register_timer(void *qtimer, void (*cb)(void*), void *data) override
	{
		Genode::Mutex::Guard guard(_timer_mutex);
		if (verbose_timer)
			Genode::log("qtimer: ", qtimer, " cb: ", cb, " data: ", data);

		Context *c = _find_context(qtimer);
		if (c != nullptr) {
			Genode::error("qtimer: ", qtimer, " already registred");
			throw -1;
		}

		_append_new_context(qtimer, cb, data);
	}

	void delete_timer(void *qtimer) override
	{
		Genode::Mutex::Guard guard(_timer_mutex);
		if (verbose_timer)
			Genode::log("qtimer: ", qtimer);

		Context *c = _find_context(qtimer);
		if (c == nullptr) {
			Genode::error("qtimer: ", qtimer, " not found");
			throw -1;
		}

		_deactivate_timer(qtimer);

		_context_list.remove(c);
		Genode::destroy(vmm_heap(), c);
	}

	void activate_timer(void *qtimer, long long int expire_abs) override
	{
		Genode::Mutex::Guard guard(_timer_mutex);
		if (verbose_timer)
			Genode::log("qtimer: ", qtimer, " expire: ", expire_abs);

		Context *c = _find_context(qtimer);
		if (c == nullptr) {
			Genode::error("qtimer: ", qtimer, " not found");
			throw -1;
		}

		c->timeout_abs_ns = expire_abs;
		c->pending        = true;

		_program_min_timer();
	}

	void deactivate_timer(void *qtimer) override
	{
		Genode::Mutex::Guard guard(_timer_mutex);
		if (verbose_timer)
			Genode::log("qtimer: ", qtimer);

		_deactivate_timer(qtimer);
	}
};


struct Pci_device : public Qemu::Pci_device
{
	PPDMDEVINS pci_dev;

	Pci_device(PPDMDEVINS pDevIns) : pci_dev(pDevIns) { }

	void raise_interrupt(int level) override {
		PDMDevHlpPCISetIrqNoWait(pci_dev, 0, level); }

	int read_dma(Qemu::addr_t addr, void *buf, Qemu::size_t size) override {
		return PDMDevHlpPhysRead(pci_dev, addr, buf, size); }

	int write_dma(Qemu::addr_t addr, void const *buf, Qemu::size_t size) override {
		return PDMDevHlpPhysWrite(pci_dev, addr, buf, size); }
};


/***********************************************
 ** Virtualbox Device function implementation **
 ***********************************************/

/**
 * @callback_method_impl{FNIOMMMIOREAD}
 */
PDMBOTHCBDECL(int) xhciMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr,
                                void *pv, unsigned cb)
{
	PXHCI pThis = PDMINS_2_DATA(pDevIns, PXHCI);

	Genode::off_t offset = GCPhysAddr - pThis->MMIOBase;
	Qemu::Controller *ctl = pThis->ctl;

	ctl->mmio_read(offset, pv, cb);
	return 0;
}


/**
 * @callback_method_impl{FNIOMMMIOWRITE}
 */
PDMBOTHCBDECL(int) xhciMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr,
                                 void const *pv, unsigned cb)
{
	PXHCI pThis = PDMINS_2_DATA(pDevIns, PXHCI);

	Genode::off_t offset = GCPhysAddr - pThis->MMIOBase;
	Qemu::Controller *ctl = pThis->ctl;

	ctl->mmio_write(offset, pv, cb);
	return 0;
}


/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) xhciR3Map(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                   uint32_t iRegion, RTGCPHYS GCPhysAddress,
                                   RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
	PXHCI pThis = (PXHCI)pPciDev;
	int rc = PDMDevHlpMMIORegister(pThis->CTX_SUFF(pDevIns), GCPhysAddress, cb, NULL /*pvUser*/,
	                               IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED
//	                               | IOMMMIO_FLAGS_DBGSTOP_ON_COMPLICATED_WRITE,
,
	                               xhciMmioWrite, xhciMmioRead, "USB XHCI");
	if (RT_FAILURE(rc))
		return rc;

	rc = PDMDevHlpMMIORegisterRC(pDevIns, GCPhysAddress, cb, NIL_RTRCPTR /*pvUser*/,
	                             "xhciMmioWrite", "xhciMmioRead");
	if (RT_FAILURE(rc))
		return rc;

	pThis->MMIOBase = GCPhysAddress;
	return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) xhciReset(PPDMDEVINS pDevIns)
{
	PXHCI pThis = PDMINS_2_DATA(pDevIns, PXHCI);

	Qemu::usb_reset();
	Qemu::usb_update_devices();
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) xhciDestruct(PPDMDEVINS pDevIns)
{
	Qemu::usb_reset();
	return 0;
}


struct Usb_ep : Genode::Entrypoint
{
	pthread_t _pthread;

	void _handle_pthread_registration()
	{
		Genode::Thread *myself = Genode::Thread::myself();
		if (!myself || Libc::pthread_create_from_thread(&_pthread, *myself, &myself)) {
			Genode::error("USB passthough will not work - thread for "
			              "pthread registration invalid");
		}
	}

	Genode::Signal_handler<Usb_ep> _pthread_reg_sigh;

	enum { USB_EP_STACK = 32u << 10, };

	Usb_ep(Genode::Env &env)
	:
		Entrypoint(env, USB_EP_STACK, "usb_ep", Genode::Affinity::Location()),
		_pthread_reg_sigh(*this, *this, &Usb_ep::_handle_pthread_registration)
	{
		Genode::Signal_transmitter(_pthread_reg_sigh).submit();
	}
};


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct,XHCI constructor}
 */
static DECLCALLBACK(int) xhciR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
	PXHCI pThis = PDMINS_2_DATA(pDevIns, PXHCI);
	PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

	pThis->usb_ep = new Usb_ep(genode_env());

	int rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, Timer_queue::tm_timer_cb,
	                                pThis, TMTIMER_FLAGS_NO_CRIT_SECT,
	                                "XHCI Timer", &pThis->controller_timer);

	static Timer_queue timer_queue(pThis->controller_timer);
	pThis->timer_queue = &timer_queue;
	static Pci_device pci_device(pDevIns);

	Genode::Attached_rom_dataspace config(genode_env(), "config");

	pThis->ctl = Qemu::usb_init(timer_queue, pci_device, *pThis->usb_ep,
	                            vmm_heap(), genode_env(), config.xml());

	Qemu::Controller::Info const ctl_info = pThis->ctl->info();

	/*
	 * Init instance data.
	 */
	pThis->pDevInsR3 = pDevIns;
	pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
	pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

	PCIDevSetVendorId      (&pThis->PciDev, ctl_info.vendor_id);
	PCIDevSetDeviceId      (&pThis->PciDev, ctl_info.product_id);
	PCIDevSetClassBase     (&pThis->PciDev, 0x0c);   /* PCI serial */
	PCIDevSetClassSub      (&pThis->PciDev, 0x03);   /* USB */
	PCIDevSetClassProg     (&pThis->PciDev, 0x30);   /* xHCI */
	PCIDevSetInterruptPin  (&pThis->PciDev, 0x01);
	PCIDevSetByte          (&pThis->PciDev, 0x60, 0x30); /* Serial Bus Release Number Register */
#ifdef VBOX_WITH_MSI_DEVICES
	PCIDevSetStatus        (&pThis->PciDev, VBOX_PCI_STATUS_CAP_LIST);
	PCIDevSetCapabilityList(&pThis->PciDev, 0x80);
#endif

	/*
	 * Register PCI device and I/O region.
	 */
	rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
	if (RT_FAILURE(rc))
		return rc;

#ifdef VBOX_WITH_MSI_DEVICES
	PDMMSIREG MsiReg;
	RT_ZERO(MsiReg);
	MsiReg.cMsiVectors    = 1;
	MsiReg.iMsiCapOffset  = 0x80;
	MsiReg.iMsiNextOffset = 0x00;
	rc = PDMDevHlpPCIRegisterMsi(pDevIns, &MsiReg);
	if (RT_FAILURE(rc))
	{
		PCIDevSetCapabilityList(&pThis->PciDev, 0x0);
		/* That's OK, we can work without MSI */
	}
#endif

	rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, pThis->ctl->mmio_size(),
	                                  PCI_ADDRESS_SPACE_MEM, xhciR3Map);
	if (RT_FAILURE(rc))
		return rc;

	return VINF_SUCCESS;
}

const PDMDEVREG g_DeviceXHCI =
{
	/* u32version */
	PDM_DEVREG_VERSION,
	/* szName */
	"nec-xhci",
	/* szRCMod */
	"VBoxDDGC.gc",
	/* szR0Mod */
	"VBoxDDR0.r0",
	/* pszDescription */
	"NEC XHCI USB controller.\n",
	/* fFlags */
	PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC,
	/* fClass */
	PDM_DEVREG_CLASS_BUS_USB,
	/* cMaxInstances */
	~0U,
	/* cbInstance */
	sizeof(XHCI),
	/* pfnConstruct */
	xhciR3Construct,
	/* pfnDestruct */
	xhciDestruct,
	/* pfnRelocate */
	NULL,
	/* pfnMemSetup */
	NULL,
	/* pfnPowerOn */
	NULL,
	/* pfnReset */
	xhciReset,
	/* pfnSuspend */
	NULL,
	/* pfnResume */
	NULL,
	/* pfnAttach */
	NULL,
	/* pfnDetach */
	NULL,
	/* pfnQueryInterface */
	NULL,
	/* pfnInitComplete */
	NULL,
	/* pfnPowerOff */
	NULL,
	/* pfnSoftReset */
	NULL,
	/* u32VersionEnd */
	PDM_DEVREG_VERSION
};


bool use_xhci_controller()
{
	try {
		Genode::Attached_rom_dataspace config(genode_env(), "config");
		return config.xml().attribute_value("xhci", false);
	} catch (Genode::Rom_connection::Rom_connection_failed) {
		return false;
	}
}
