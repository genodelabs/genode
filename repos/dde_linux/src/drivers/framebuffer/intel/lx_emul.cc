/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Norman Feske
 * \date   2015-08-19
 */

/* local includes */
#include "lx_emul_private.h"

/* Genode includes */
#include <base/printf.h>

/* DRM-specific includes */
extern "C" {
#include <drm/drmP.h>
#include <i915/i915_drv.h>
#include <i915/intel_drv.h>
}


void lx_printf(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
}


void lx_vprintf(char const *fmt, va_list va)
{
	Genode::vprintf(fmt, va);
}


/****************************************
 ** Common Linux kernel infrastructure **
 ****************************************/

#include <lx_emul/impl/kernel.h>

int oops_in_progress;


/********************
 ** linux/string.h **
 ********************/

char *strncpy(char *dst, const char* src, size_t n)
{
	return Genode::strncpy(dst, src, n);
}


/*****************
 ** linux/dmi.h **
 *****************/

int dmi_check_system(const struct dmi_system_id *list)
{
	TRACE;
	/*
	 * Is used to check for quirks of the platform.
	 */
	return 0;
}


/*******************
 ** linux/delay.h **
 *******************/

#include <lx_emul/impl/delay.h>


/**********************
 ** Global variables **
 **********************/

struct task_struct *current;


struct boot_cpu_data boot_cpu_data;


/*******************
 ** Kernel memory **
 *******************/

#include <lx_emul/impl/slab.h>
#include <lx_emul/impl/gfp.h>


dma_addr_t page_to_phys(void *p)
{
	struct page *page = (struct page *)p;

	dma_addr_t const phys = Lx::Malloc::dma().phys_addr(page->addr);

	PDBG("virt=0x%lx -> phys=0x%lx", (long)page->addr, phys);
	return phys;
}


/*****************
 ** linux/idr.h **
 *****************/

void idr_init(struct idr *idp)
{
	TRACE;
}


int idr_alloc(struct idr *idp, void *ptr, int start, int end, gfp_t gfp_mask)
{
	TRACE;
	/*
	 * Called from i915_gem_context.c: create_hw_context()
	 */
	return 0;
}


int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end, gfp_t gfp_mask)
{
	/*
	 * Called by drm_crtc.c: drm_connector_init to allocate a connector_type_id
	 */
	TRACE;
	return 0;
}



/**********************
 ** asm/cacheflush.h **
 **********************/

int set_pages_uc(struct page *page, int numpages)
{
	TRACE;
	return 0;
}


/********************
 ** linux/ioport.h **
 ********************/

struct resource iomem_resource;


/*********
 ** PCI **
 *********/

#include <lx_emul/impl/io.h>
#include <lx_emul/impl/pci.h>

struct pci_dev *pci_get_bus_and_slot(unsigned int bus, unsigned int devfn)
{
	if (bus != 0 || devfn != 0)
		TRACE_AND_STOP;

	pci_dev *pci_dev = nullptr;

	auto lamda = [&] (Platform::Device_capability cap) {

		Platform::Device_client client(cap);

		/* request bus address of device from platform driver */
		unsigned char dev_bus = 0, dev_slot = 0, dev_fn = 0;
		client.bus_address(&dev_bus, &dev_slot, &dev_fn);

		if (dev_bus == bus && PCI_SLOT(devfn) == dev_slot && PCI_FUNC(devfn) == dev_fn) {
			Lx::Pci_dev *dev = new (Genode::env()->heap()) Lx::Pci_dev(cap);
			Lx::pci_dev_registry()->insert(dev);
			pci_dev = dev;
			return true;
		}

		return false;
	};

	Lx::for_each_pci_device(lamda);

	return pci_dev;
}


struct pci_dev *pci_get_class(unsigned int class_code, struct pci_dev *from)
{
	/*
	 * The function is solely called by the i915 initialization code to
	 * probe for the ISA bridge device in order to detect the hardware
	 * generation.
	 *
	 * We look up the bridge but don't need to support the iteration over
	 * multiple devices of the given class.
	 */
	if (from)
		return nullptr;

	pci_dev *pci_dev = nullptr;

	auto lamda = [&] (Platform::Device_capability cap) {

		Platform::Device_client client(cap);

		if (client.class_code() == class_code) {
			pci_dev = new (Genode::env()->heap()) Lx::Pci_dev(cap);
			return true;
		}

		return false;
	};

	Lx::for_each_pci_device(lamda);

	return pci_dev;
}


void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long max)
{
	return pci_ioremap_bar(dev, bar);
}


struct pci_dev *pci_dev_get(struct pci_dev *dev)
{
	TRACE;
	return dev;
};


int vga_get_uninterruptible(struct pci_dev *pdev, unsigned int rsrc)
{
	/*
	 * This function locks the VGA device. It is normally provided by the
	 * VGA arbiter in the Linux kernel. We don't need this arbitration because
	 * the platform-driver enforces the exclusive access to the VGA resources
	 * by our driver.
	 *
	 * At the time when this function is called, the 'pci_dev' structure for
	 * the VGA card was already requested. Hence, subsequent I/O accesses
	 * should work.
	 */
	TRACE;
	return 0;
}


void vga_put(struct pci_dev *pdev, unsigned int rsrc)
{
	TRACE;
}


int pci_bus_alloc_resource(struct pci_bus *, struct resource *, resource_size_t,
                           resource_size_t, resource_size_t, unsigned int,
                           resource_size_t (*)(void *, const struct resource *, resource_size_t, resource_size_t),
                           void *alignf_data)
{
	TRACE;
	return 0;
}


void pci_set_master(struct pci_dev *dev)
{
	TRACE;
}


int pci_enable_msi(struct pci_dev *dev)
{
	TRACE;
	return 0;
}



struct io_mapping
{
	private:

		resource_size_t _base;
		unsigned long   _size;

	public:

		/**
		 * Constructor
		 */
		io_mapping(resource_size_t base, unsigned long size) :
			_base(base), _size(size) { }

		resource_size_t base() const { return _base; }
};


/**
 * I/O mapping used by i915_dma.c to map the GTT aperture
 */
struct io_mapping *io_mapping_create_wc(resource_size_t base, unsigned long size)
{
	static int called;
	TRACE;

	if (called++ != 0) {
		PERR("io_mapping_create_wc unexpectedly called twice");
		return 0;
	}

	io_mapping *mapping = new (Genode::env()->heap()) io_mapping(base, size);
	return mapping;
}


/****************
 ** linux/io.h **
 ****************/

int arch_phys_wc_add(unsigned long base, unsigned long size)
{
	/*
	 * Linux tries to manipulate physical memory attributes here (e.g.,
	 * using MTRRs). But when using PAT, this is not needed. When running
	 * on top of a microkernel, we cannot manipulate the attributes
	 * anyway.
	 */
	TRACE;
	return 0;
}


/********************
 ** linux/device.h **
 ********************/

struct subsys_private { int dummy; };

int bus_register(struct bus_type *bus)
{
	/*
	 * called by i2c-core init
	 *
	 * The subsequent code checks for the 'p' member of the 'bus'. So
	 * we have to supply a valid pointer there.
	 */

	static subsys_private priv = { 0 };
	bus->p = &priv;

	TRACE;
	return 0;
}


int driver_register(struct device_driver *drv)
{
	TRACE;
	return 0;
}


int bus_for_each_dev(struct bus_type *bus, struct device *start, void *data,
                     int (*fn)(struct device *dev, void *data))
{
	/*
	 * Called bu the i2c-core driver after registering the driver. This
	 * function is called to process drivers that are present at initialization
	 * time. Since we initialize the i2c driver prior the others, we don't
	 * need to perform anything.
	 */
	TRACE;
	return 0;
}


int dev_set_name(struct device *dev, const char *name, ...)
{
	PDBG("name=%s", name);
	TRACE;
	return 0;
}


int device_register(struct device *dev)
{
	TRACE;
	return 0;
}


/***********************
 ** linux/workqueue.h **
 ***********************/

struct workqueue_struct *create_singlethread_workqueue(char const *)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	return wq;
}

struct workqueue_struct *alloc_ordered_workqueue(char const *name , unsigned int flags, ...)
{
	return create_singlethread_workqueue(name);
}


/***************
 ** Execution **
 ***************/

#include <lx_emul/impl/spinlock.h>
#include <lx_emul/impl/mutex.h>
#include <lx_emul/impl/sched.h>
#include <lx_emul/impl/timer.h>
#include <lx_emul/impl/completion.h>
#include <lx_emul/impl/wait.h>


void __wait_completion(struct completion *work)
{
	TRACE_AND_STOP;
}


void mutex_lock_nest_lock(struct mutex *lock, struct mutex *)
{
	TRACE;
	/*
	 * Called by drm_crtc.c: drm_modeset_lock_all, drm_crtc_init to
	 * lock the crtc mutex.
	 */
	mutex_lock(lock);
}


/************************
 ** DRM implementation **
 ************************/

unsigned int drm_debug = 1;


extern "C" int drm_pci_init(struct drm_driver *driver, struct pci_driver *pdriver)
{
	PDBG("call pci_register_driver");

	return pci_register_driver(pdriver);
}


struct drm_device *drm_dev_alloc(struct drm_driver *driver, struct device *parent)
{
	struct drm_device *dev = (drm_device *)kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return nullptr;

	dev->driver = driver;

	spin_lock_init(&dev->count_lock);
	spin_lock_init(&dev->event_lock);

	mutex_init(&dev->struct_mutex);

//	ret = drm_gem_init(dev);
//	if (ret) {
//		PERR("drm_gem_init failed");
//		return nullptr;
//	}

	return dev;
}


/*
 * Called indirectly when 'pci_register_driver' has found a matching
 * device.
 */
int drm_get_pci_dev(struct pci_dev *pdev, const struct pci_device_id *ent,
                    struct drm_driver *driver)
{
	drm_device *dev = drm_dev_alloc(driver, &pdev->dev);
	if (!dev)
		return -ENOMEM;

//	ret = pci_enable_device(pdev);

	dev->pdev = pdev;

	pci_set_drvdata(pdev, dev);

//	drm_pci_agp_init(dev);

	/*
	 * Kick off the actual driver initialization code
	 *
	 * In the Linux DRM code, this happens indirectly via the call of
	 * 'drm_dev_register'.
	 */
	driver->load(dev, ent->driver_data);

	DRM_INFO("Initialized %s %d.%d.%d %s for %s on minor %d\n",
		 driver->name, driver->major, driver->minor, driver->patchlevel,
		 driver->date, pci_name(pdev), dev->primary->index);

	return 0;
}


/**
 * Called from 'i915_driver_load'
 */
int drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	TRACE;
	return 0;
}


int drm_irq_install(struct drm_device *dev)
{
	TRACE;
	return 0;
}


/**************************************
 ** Stubs for non-ported driver code **
 **************************************/

void intel_pm_setup(struct drm_device *dev)
{
	TRACE;
}

void intel_init_pm(struct drm_device *dev)
{
	TRACE;
}

int intel_power_domains_init(struct drm_device *dev)
{
	TRACE;
	return 0;
}

void intel_power_domains_init_hw(struct drm_device *dev)
{
	TRACE;
}

void pm_qos_add_request(struct pm_qos_request *req, int pm_qos_class, s32 value)
{
	TRACE;
}

void intel_disable_gt_powersave(struct drm_device *dev)
{
	TRACE;
}

void intel_setup_bios(struct drm_device *dev)
{
	TRACE;
}

int intel_parse_bios(struct drm_device *dev)
{
	TRACE;
	return -1;
}

void i915_gem_detect_bit_6_swizzle(struct drm_device *dev)
{
	TRACE;
}

int register_shrinker(struct shrinker *)
{
	TRACE;
	return 0;
}

int vga_client_register(struct pci_dev *pdev, void *cookie, void (*irq_set_state)(void *cookie, bool state), unsigned int (*set_vga_decode)(void *cookie, bool state))
{
	TRACE;
	return -ENODEV;
}

int vga_switcheroo_register_client(struct pci_dev *dev, const struct vga_switcheroo_client_ops *ops, bool driver_power_control)
{
	TRACE;
	return 0;
}

int intel_plane_init(struct drm_device *dev, enum pipe pipe, int plane)
{
	TRACE;
	return 0;
}

struct i915_ctx_hang_stats * i915_gem_context_get_hang_stats(struct drm_device *dev, struct drm_file *file, u32 id)
{
	TRACE_AND_STOP;
	return NULL;
}

struct resource * devm_request_mem_region(struct device *dev, resource_size_t start, resource_size_t n, const char *name)
{
	/*
	 * This function solely called for keeping the stolen memory preserved
	 * for the driver only ('i915_stolen_to_physical'). The returned pointer is
	 * just checked for NULL but not used otherwise.
	 */
	TRACE;
	static struct resource dummy;
	return &dummy;
}


DEFINE_SPINLOCK(mchdev_lock);
