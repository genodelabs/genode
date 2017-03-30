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
#include <util/list.h>

/* qemu-usb includes */
#include <qemu/usb.h>

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
struct Destruction_helper;


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

	/** Receiver thread that handles all USB signals. */
	PPDMTHREAD   pThread;

	PTMTIMERR3        controller_timer;
	Timer_queue      *timer_queue;
	Qemu::Controller *ctl;

	Genode::Signal_receiver *usb_sig_rec;
	Destruction_helper      *destruction_helper;
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
			if (c->timeout_abs_ns < min->timeout_abs_ns && c->pending)
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

		TMTimerSetNano(tm_timer, min->timeout_abs_ns - TMTimerGetNano(tm_timer));
	}

	void _deactivate_timer(void *qtimer)
	{
		Context *c = _find_context(qtimer);
		if (c == nullptr) {
			Genode::error("qtimer: ", qtimer, " not found");
			throw -1;
		}

		if (c->pending) {
			Context *min = _min_pending();
			if (min == c) {
				TMTimerStop(tm_timer);
				_program_min_timer();
			}
		}

		c->pending = false;
	}

	Timer_queue(PTMTIMER timer) : tm_timer(timer) { }

	void timeout()
	{
		uint64_t now = TMTimerGetNano(tm_timer);

		for (Context *c = _context_list.first(); c; c = c->next()) {
			if (c->pending && c->timeout_abs_ns <= now) {
				Qemu::usb_timer_callback(c->cb, c->data);
				c->pending = false;
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

	Genode::Lock _timer_lock;

	void register_timer(void *qtimer, void (*cb)(void*), void *data) override
	{
		Genode::Lock::Guard lock_guard(_timer_lock);
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
		Genode::Lock::Guard lock_guard(_timer_lock);
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
		Genode::Lock::Guard lock_guard(_timer_lock);
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
		Genode::Lock::Guard lock_guard(_timer_lock);
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

	void *map_dma(Qemu::addr_t base, Qemu::size_t size) override
	{
		PGMPAGEMAPLOCK lock;
		void * vmm_addr = nullptr;

		int rc = PDMDevHlpPhysGCPhys2CCPtr(pci_dev, base, 0, &vmm_addr, &lock);
		Assert(rc == VINF_SUCCESS);

		/* the mapping doesn't go away, so release internal lock immediately */
		PDMDevHlpPhysReleasePageMappingLock(pci_dev, &lock);
		return vmm_addr;
	}

	void unmap_dma(void *addr, Qemu::size_t size) override { }
};


/*************************************
 ** Qemu::Usb signal thread backend **
 *************************************/

struct Destruction_helper
{
	Genode::Signal_receiver &sig_rec;

	Genode::Signal_dispatcher<Destruction_helper> dispatcher {
		sig_rec, *this, &Destruction_helper::handle };

	void handle(unsigned) { }

	Destruction_helper(Genode::Signal_receiver &sig_rec)
	: sig_rec(sig_rec) { }
};


static DECLCALLBACK(int) usb_signal_thread(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
	PXHCI pThis = PDMINS_2_DATA(pDevIns, PXHCI);
	if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
		return VINF_SUCCESS;

	while (pThread->enmState == PDMTHREADSTATE_RUNNING)
	{
		Genode::Signal sig = pThis->usb_sig_rec->wait_for_signal();
		int num            = sig.num();

		Genode::Signal_dispatcher_base *dispatcher;
		dispatcher = dynamic_cast<Genode::Signal_dispatcher_base *>(sig.context());
		if (dispatcher) {
			dispatcher->dispatch(num);
		}
	}

	return VINF_SUCCESS;
}


static DECLCALLBACK(int) usb_signal_thread_wakeup(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
	PXHCI pThis = PDMINS_2_DATA(pDevIns, PXHCI);

	Genode::Signal_transmitter(pThis->destruction_helper->dispatcher).submit();

	return VINF_SUCCESS;
}


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


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct,XHCI constructor}
 */
static DECLCALLBACK(int) xhciR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
	PXHCI pThis = PDMINS_2_DATA(pDevIns, PXHCI);
	PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

	pThis->usb_sig_rec        = new (vmm_heap()) Genode::Signal_receiver();
	pThis->destruction_helper = new (vmm_heap())
	                                Destruction_helper(*(pThis->usb_sig_rec));

	int rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, Timer_queue::tm_timer_cb,
	                                pThis, TMTIMER_FLAGS_NO_CRIT_SECT,
	                                "NEC-XHCI Timer", &pThis->controller_timer);

	static Timer_queue timer_queue(pThis->controller_timer);
	pThis->timer_queue = &timer_queue;
	static Pci_device pci_device(pDevIns);

	rc = PDMDevHlpThreadCreate(pDevIns, &pThis->pThread, pThis,
	                           usb_signal_thread, usb_signal_thread_wakeup,
	                           32 * 1024 , RTTHREADTYPE_IO, "usb_signal");

	pThis->ctl = Qemu::usb_init(timer_queue, pci_device, *pThis->usb_sig_rec,
	                            vmm_heap(), genode_env());

	/*
	 * Init instance data.
	 */
	pThis->pDevInsR3 = pDevIns;
	pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
	pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

	PCIDevSetVendorId      (&pThis->PciDev, 0x1033); /* PCI_VENDOR_ID_NEC */
	PCIDevSetDeviceId      (&pThis->PciDev, 0x0194); /* PCI_DEVICE_ID_NEC_UPD720200 */
	PCIDevSetClassProg     (&pThis->PciDev, 0x30);   /* xHCI */
	PCIDevSetClassSub      (&pThis->PciDev, 0x03);
	PCIDevSetClassBase     (&pThis->PciDev, 0x0c);
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
