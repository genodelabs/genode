/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Norman Feske
 * \date   2015-08-19
 */

/* Genode includes */
#include <base/printf.h>
#include <os/attached_io_mem_dataspace.h>
#include <os/config.h>
#include <os/reporter.h>

/* local includes */
#include <component.h>
#include "lx_emul_private.h"

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

int strncmp(const char *cs, const char *ct, size_t count)
{
	return Genode::strcmp(cs, ct, count);
}

int memcmp(const void *cs, const void *ct, size_t count)
{
	/* original implementation from lib/string.c */
	const unsigned char *su1, *su2;
	int res = 0;

	for (su1 = (unsigned char*)cs, su2 = (unsigned char*)ct;
	     0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}

size_t strlen(const char *s)
{
	return Genode::strlen(s);
}

long simple_strtol(const char *cp, char **endp, unsigned int base)
{
	unsigned long result = 0;
	size_t ret = Genode::ascii_to_unsigned(cp, result, base);
	if (endp) *endp = (char*)cp + ret;
	return result;
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


/*******************************
 ** kernel/time/timekeeping.c **
 *******************************/

void getrawmonotonic(struct timespec *ts)
{
	unsigned long ms = _delay_timer.elapsed_ms();
	ts->tv_sec  = ms / 1000;
	ts->tv_nsec = (ms - ts->tv_sec*1000) * 1000000;
}


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


dma_addr_t page_to_phys(struct page *page)
{
	return page->paddr;
}

void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags)
{
	return kmem_cache_alloc(k, flags | __GFP_ZERO);
}


/*****************
 ** linux/idr.h **
 *****************/

void idr_init(struct idr *idp)
{
	Genode::memset(idp, 0, sizeof(struct idr));
}

static Genode::Bit_allocator<1024> id_allocator;

int idr_alloc(struct idr *idp, void *ptr, int start, int end, gfp_t gfp_mask)
{
	int max = end > 0 ? end - 1 : ((int)(~0U>>1));  /* inclusive upper limit */
	int id;

	/* sanity checks */
	if (start < 0)   return -EINVAL;
	if (max < start) return -ENOSPC;

	/* allocate id */
	id = id_allocator.alloc();
	if (id > max) return -ENOSPC;

	ASSERT(id >= start);
	return id;
}


int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end, gfp_t gfp_mask)
{
	int max = end > 0 ? end - 1 : ((int)(~0U>>1));
	int id  = id_allocator.alloc();
	if (id > max) return -ENOSPC;

	ASSERT((unsigned int)id >= start);
	return id;
}


void ida_remove(struct ida *ida, int id)
{
	id_allocator.free(id);
}


void idr_remove(struct idr *idp, int id)
{
	id_allocator.free(id);
}


void *idr_find(struct idr *idr, int id)
{
	TRACE;
	return NULL;
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


dma_addr_t pci_map_page(struct pci_dev *hwdev, struct page *page,
                        unsigned long offset, size_t size, int direction) {
	return page->paddr + offset;
}


int pci_dma_mapping_error(struct pci_dev *pdev, dma_addr_t dma_addr)
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


void iounmap(volatile void *addr)
{
	/* do not unmap here, but when client requests new dataspace */
	TRACE;
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


/*******************************
 ** arch/x86/include/asm/io.h **
 *******************************/

void memset_io(void *addr, int val, size_t count)
{
	memset((void __force *)addr, val, count);
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


/**
 * Assuming that driver_register is only called for i2c device driver
 * registration, we can store its pointer here
 */
static struct device_driver *i2c_device_driver = nullptr;

int driver_register(struct device_driver *drv)
{
	TRACE;
	ASSERT(!i2c_device_driver);
	i2c_device_driver = drv;
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


int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,
                     void *data, int (*fn)(struct device_driver *, void *))
{
	TRACE;
	return fn(i2c_device_driver, data);
}


/***********************
 ** linux/workqueue.h **
 ***********************/

struct workqueue_struct *system_wq;

/** needed by workqueue backend implementation **/
struct tasklet_struct {
	void (*func)(unsigned long);
	unsigned long data;
};

#include <lx_emul/impl/work.h>

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


bool in_atomic()
{
	TRACE;
	return false;
}


bool irqs_disabled()
{
	TRACE;
	return false;
}


void usleep_range(unsigned long min, unsigned long max)
{
	udelay(min);
}


unsigned long round_jiffies_up_relative(unsigned long j)
{
	j += jiffies;
	return j - (j%HZ) + HZ;
}


/************************
 ** DRM implementation **
 ************************/

#include <lx_emul/impl/internal/irq.h>

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


static void drm_get_minor(struct drm_device *dev, struct drm_minor **minor, int type)
{
	// int minor_id = drm_minor_get_id(dev, type);

	struct drm_minor *new_minor = (struct drm_minor*)
		kzalloc(sizeof(struct drm_minor), GFP_KERNEL);
	ASSERT(new_minor);
	new_minor->type = type;
	//new_minor->device = MKDEV(DRM_MAJOR, minor_id);
	new_minor->dev = dev;
	//new_minor->index = minor_id;
	//INIT_LIST_HEAD(&new_minor->master_list);
	*minor = new_minor;
}

static struct drm_device * singleton_drm_device = nullptr;

static void drm_dev_register(struct drm_device *dev, unsigned long flags)
{
	//if (drm_core_check_feature(dev, DRIVER_MODESET))
	//	drm_get_minor(dev, &dev->control, DRM_MINOR_CONTROL);

	//if (drm_core_check_feature(dev, DRIVER_RENDER) && drm_rnodes)
	//	drm_get_minor(dev, &dev->render, DRM_MINOR_RENDER);

	drm_get_minor(dev, &dev->primary, DRM_MINOR_LEGACY);

	ASSERT(!singleton_drm_device);
	singleton_drm_device = dev;
	ASSERT(!dev->driver->load(dev, flags));
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
	drm_dev_register(dev, ent->driver_data);

	DRM_INFO("Initialized %s %d.%d.%d %s for %s on minor %d\n",
		 driver->name, driver->major, driver->minor, driver->patchlevel,
		 driver->date, pci_name(pdev), dev->primary->index);

	return 0;
}


static void vblank_disable_fn(unsigned long arg)
{
	struct drm_device *dev = (struct drm_device *)arg;
	unsigned long irqflags;
	int i = 0;

	if (!dev->vblank_disable_allowed)
		return;

	//for (i = 0; i < dev->num_crtcs; i++) {
		spin_lock_irqsave(&dev->vbl_lock, irqflags);
	//	if (atomic_read(&dev->vblank_refcount[i]) == 0 &&
	//		dev->vblank_enabled[i]) {
	//		DRM_DEBUG("disabling vblank on crtc %d\n", i);
	//		dev->last_vblank[i] =
	//			dev->driver->get_vblank_counter(dev, i);
			dev->driver->disable_vblank(dev, i);
			//dev->vblank_enabled[i] = 0;
	//	}
	spin_unlock_irqrestore(&dev->vbl_lock, irqflags);
	//}
}

/**
 * Called from 'i915_driver_load'
 */
int drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	setup_timer(&dev->vblank_disable_timer, vblank_disable_fn,
				(unsigned long)dev);
	spin_lock_init(&dev->vbl_lock);
	dev->vblank = (drm_vblank_crtc*) kzalloc(num_crtcs * sizeof(*dev->vblank), GFP_KERNEL);
	dev->vblank_disable_allowed = 0;
	return 0;
}


void drm_vblank_pre_modeset(struct drm_device *dev, int crtc)
{
	/* Enable vblank irqs under vblank_time_lock protection.
	 * All vblank count & timestamp updates are held off
	 * until we are done reinitializing master counter and
	 * timestamps. Filtercode in drm_handle_vblank() will
	 * prevent double-accounting of same vblank interval.
	 */
	int ret = dev->driver->enable_vblank(dev, crtc);
	DRM_DEBUG("enabling vblank on crtc %d, ret: %d\n",
			  crtc, ret);
	//drm_update_vblank_count(dev, crtc);
}


void drm_vblank_post_modeset(struct drm_device *dev, int crtc)
{
	dev->vblank_disable_allowed = true;

	//if (drm_vblank_offdelay > 0)
	if (dev->vblank_disable_timer.function == 0) PERR("NO TIMER FUNC");
	mod_timer(&dev->vblank_disable_timer,
			  jiffies + ((5000/*drm_vblank_offdelay*/ * HZ)/1000));
}


int drm_irq_install(struct drm_device *dev)
{
	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EINVAL;

	if (dev->irq_enabled)
		return -EBUSY;

	dev->irq_enabled = true;

	if (dev->driver->irq_preinstall)
		dev->driver->irq_preinstall(dev);

	/* enable IRQ */
	Lx::Pci_dev * pci_dev = (Lx::Pci_dev*) dev->pdev->bus;
	Lx::Irq::irq().request_irq(pci_dev->client(), dev->driver->irq_handler, (void*)dev);

	/* After installing handler */
	int ret = 0;
	if (dev->driver->irq_postinstall)
		ret = dev->driver->irq_postinstall(dev);

	return ret;
}


void drm_calc_timestamping_constants(struct drm_crtc *crtc, const struct drm_display_mode *mode)
{
	int linedur_ns = 0, pixeldur_ns = 0, framedur_ns = 0;
	int dotclock = mode->crtc_clock;

	/* Valid dotclock? */
	if (dotclock > 0) {
		int frame_size = mode->crtc_htotal * mode->crtc_vtotal;

		/*
		 * Convert scanline length in pixels and video
		 * dot clock to line duration, frame duration
		 * and pixel duration in nanoseconds:
		 */
		pixeldur_ns = 1000000 / dotclock;
		linedur_ns  = div_u64((u64) mode->crtc_htotal * 1000000, dotclock);
		framedur_ns = div_u64((u64) frame_size * 1000000, dotclock);

		/*
		 * Fields of interlaced scanout modes are only half a frame duration.
		 */
		if (mode->flags & DRM_MODE_FLAG_INTERLACE)
			framedur_ns /= 2;
	} else
		DRM_ERROR("crtc %d: Can't calculate constants, dotclock = 0!\n",
				  crtc->base.id);

	crtc->pixeldur_ns = pixeldur_ns;
	crtc->linedur_ns  = linedur_ns;
	crtc->framedur_ns = framedur_ns;

	DRM_DEBUG("crtc %d: hwmode: htotal %d, vtotal %d, vdisplay %d\n",
			  crtc->base.id, mode->crtc_htotal,
			  mode->crtc_vtotal, mode->crtc_vdisplay);
	DRM_DEBUG("crtc %d: clock %d kHz framedur %d linedur %d, pixeldur %d\n",
			  crtc->base.id, dotclock, framedur_ns,
			  linedur_ns, pixeldur_ns);
	TRACE;
}


void drm_gem_private_object_init(struct drm_device *dev, struct drm_gem_object *obj, size_t size)
{
	obj->dev = dev;
	obj->filp = NULL;

	//kref_init(&obj->refcount);
	//obj->handle_count = 0;
	obj->size = size;
	//drm_vma_node_reset(&obj->vma_node);
}


/***************************
 ** arch/x86/kernel/tsc.c **
 ***************************/

unsigned int tsc_khz;


/**************************************
 ** arch/x86/include/asm/processor.h **
 **************************************/

void cpu_relax(void)
{
	Lx::timer_update_jiffies();
	asm volatile("rep; nop" ::: "memory");
}


/***********************
 ** linux/workqueue.h **
 ***********************/

bool mod_delayed_work(struct workqueue_struct *, struct delayed_work *, unsigned long)
{
	TRACE;
	return false;
}


/********************
 ** kernel/panic.c **
 ********************/

struct atomic_notifier_head panic_notifier_list;
int panic_timeout;


/***********************
 ** drivers/pci/rom.c **
 ***********************/

void __iomem __must_check *pci_map_rom(struct pci_dev *pdev, size_t *size)
{
	enum { VIDEO_ROM_BASE = 0xC0000, VIDEO_ROM_SIZE = 0x20000 };

	static Genode::Attached_io_mem_dataspace vrom(VIDEO_ROM_BASE, VIDEO_ROM_SIZE);
	*size = VIDEO_ROM_SIZE;
	return vrom.local_addr<void*>();
}

void pci_unmap_rom(struct pci_dev *pdev, void __iomem *rom) {}


/***********************
 ** lib/scatterlist.c **
 ***********************/

void sg_mark_end(struct scatterlist *sg)
{
	sg->page_link |= 0x02;
	sg->page_link &= ~0x01;
}


int sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp_mask)
{
	enum { MAX_ENTS = 4096 / sizeof(struct scatterlist) };

	ASSERT(nents <= MAX_ENTS);

	Genode::memset(table, 0, sizeof(*table));

	struct scatterlist *sg = (scatterlist*)
		kmalloc(nents * sizeof(struct scatterlist), gfp_mask);
	if (!sg) return -ENOMEM;

	Genode::memset(sg, 0, sizeof(*sg) * nents);
	table->nents = nents;
	table->sgl = sg;
	sg_mark_end(&sg[nents - 1]);
	return 0;
}

static inline bool sg_is_chain(struct scatterlist* sg) {
	return ((sg)->page_link & 0x01); }

	static inline bool sg_is_last(struct scatterlist* sg) {
		return ((sg)->page_link & 0x02); }

		static inline struct scatterlist* sg_chain_ptr(struct scatterlist* sg) {
			return (struct scatterlist *) ((sg)->page_link & ~0x03); }

struct scatterlist *sg_next(struct scatterlist * sg)
{
	if (sg_is_last(sg))
		return NULL;

	sg++;
	if (unlikely(sg_is_chain(sg)))
		sg = sg_chain_ptr(sg);

	return sg;
}

void __sg_page_iter_start(struct sg_page_iter *piter, struct scatterlist *sglist, unsigned int nents, unsigned long pgoffset)
{
	piter->__pg_advance = 0;
	piter->__nents = nents;
	piter->sg = sglist;
	piter->sg_pgoffset = pgoffset;
}

static int sg_page_count(struct scatterlist *sg) {
	return Genode::align_addr(sg->offset + sg->length, 12) >> PAGE_SHIFT; }

bool __sg_page_iter_next(struct sg_page_iter *piter)
{
	if (!piter->__nents || !piter->sg)
		return false;

	piter->sg_pgoffset += piter->__pg_advance;
	piter->__pg_advance = 1;

	while (piter->sg_pgoffset >= (unsigned long)sg_page_count(piter->sg)) {
		piter->sg_pgoffset -= sg_page_count(piter->sg);
		piter->sg = sg_next(piter->sg);
		if (!--piter->__nents || !piter->sg)
			return false;
	}

	return true;
}


dma_addr_t sg_page_iter_dma_address(struct sg_page_iter *piter)
{
	return sg_dma_address(piter->sg) + (piter->sg_pgoffset << PAGE_SHIFT);
}


/******************
 ** linux/kref.h **
 ******************/

void kref_init(struct kref *kref) {
	kref->refcount.counter = 1; }

void kref_get(struct kref *kref) {
	kref->refcount.counter++; }

int kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	kref->refcount.counter--;
	if (kref->refcount.counter == 0) {
		release(kref);
		return 1;
	}
	return 0;
}


/*****************************
 ** drivers/video/fbsysfs.c **
 *****************************/

struct fb_info *framebuffer_alloc(size_t size, struct device *dev)
{
	static constexpr int BYTES_PER_LONG = BITS_PER_LONG / 8;
	static constexpr int PADDING =
		BYTES_PER_LONG - (sizeof(struct fb_info) % BYTES_PER_LONG);

	int fb_info_size = sizeof(struct fb_info);
	struct fb_info *info;
	char *p;

	if (size)
		fb_info_size += PADDING;

	p = (char*)kzalloc(fb_info_size + size, GFP_KERNEL);

	if (!p)
		return NULL;

	info = (struct fb_info *) p;

	if (size)
		info->par = p + fb_info_size;

	//info->device = dev;

	return info;
}

void framebuffer_release(struct fb_info *info)
{
	kfree(info);
}


/****************
 ** linux/fb.h **
 ****************/

struct apertures_struct *alloc_apertures(unsigned int max_num)
{
	struct apertures_struct *a = (struct apertures_struct*)
		kzalloc(sizeof(struct apertures_struct)
		        + max_num * sizeof(struct aperture), GFP_KERNEL);
	if (!a)
		return NULL;
	a->count = max_num;
	return a;
}

extern "C" void update_framebuffer_config()
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private*)singleton_drm_device->dev_private;
	struct intel_framebuffer * ifb = &dev_priv->fbdev->ifb;

	struct drm_connector *connector;
	list_for_each_entry(connector, &singleton_drm_device->mode_config.connector_list, head)
		connector->force = DRM_FORCE_UNSPECIFIED;
	intel_fbdev_fini(singleton_drm_device);
	i915_gem_object_release_stolen(ifb->obj);
	drm_mode_config_reset(singleton_drm_device);
	intel_fbdev_init(singleton_drm_device);
	intel_fbdev_initial_config(singleton_drm_device);
}

static Genode::addr_t new_fb_ds_base = 0;
static Genode::addr_t cur_fb_ds_base = 0;
static Genode::size_t cur_fb_ds_size = 0;

Genode::Dataspace_capability Framebuffer::framebuffer_dataspace()
{
	if (cur_fb_ds_base)
		Lx::iounmap((void*)cur_fb_ds_base);
	cur_fb_ds_base = new_fb_ds_base;
	return Lx::ioremap_lookup(cur_fb_ds_base, cur_fb_ds_size);
}

int register_framebuffer(struct fb_info *fb_info)
{
	using namespace Genode;

	fb_info->fbops->fb_set_par(fb_info);
	new_fb_ds_base = (addr_t)fb_info->screen_base;
	cur_fb_ds_size = (size_t)fb_info->screen_size;
	Framebuffer::root->update(fb_info->var.yres_virtual, fb_info->fix.line_length / 2);
	return 0;
}

int unregister_framebuffer(struct fb_info *fb_info)
{
	TRACE;
	return 0;
}


/*********************************************
 ** drivers/gpu/drm/i915/intel_ringbuffer.c **
 *********************************************/

int intel_init_render_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[0];
	ring->dev = dev;
	return 0;
}


/**************************************
 ** Stubs for non-ported driver code **
 **************************************/

void pm_qos_add_request(struct pm_qos_request *req, int pm_qos_class, s32 value)
{
	TRACE;
}

void pm_qos_update_request(struct pm_qos_request *req, s32 new_value)
{
	TRACE;
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

int acpi_lid_notifier_register(struct notifier_block *nb)
{
	TRACE;
	return 0;
}

void update_genode_report()
{
	static Genode::Reporter reporter("connectors");

	try {
		Genode::config()->reload();
		reporter.enabled(Genode::config()->xml_node().sub_node("report")
		                 .attribute_value(reporter.name().string(), false));
	} catch (...) {
		reporter.enabled(false);
	}

	if (!reporter.is_enabled()) return;

	try {

		Genode::Reporter::Xml_generator xml(reporter, [&] ()
		{
			struct drm_device *dev = singleton_drm_device;
			struct drm_connector *connector;
			struct list_head panel_list;

			INIT_LIST_HEAD(&panel_list);

			list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
				xml.node("connector", [&] ()
				{
					bool connected = connector->status == connector_status_connected;
					xml.attribute("name", drm_get_connector_name(connector));
					xml.attribute("connected", connected);

					struct drm_display_mode *mode;
					struct list_head mode_list;
					INIT_LIST_HEAD(&mode_list);
					list_for_each_entry(mode, &connector->modes, head) {
						xml.node("mode", [&] ()
						{
							xml.attribute("width", mode->hdisplay);
							xml.attribute("height", mode->vdisplay);
							xml.attribute("hz", mode->vrefresh);
						});
					}
					INIT_LIST_HEAD(&mode_list);
					list_for_each_entry(mode, &connector->probed_modes, head) {
						xml.node("mode", [&] ()
						{
							xml.attribute("width", mode->hdisplay);
							xml.attribute("height", mode->vdisplay);
							xml.attribute("hz", mode->vrefresh);
						});
					}
				});
			}
		});
	} catch (...) {
		PWRN("Failed to generate report");
	}
}

int drm_sysfs_connector_add(struct drm_connector *connector)
{
	TRACE;
	return 0;
}

void drm_sysfs_connector_remove(struct drm_connector *connector)
{
	TRACE;
}

void assert_spin_locked(spinlock_t *lock)
{
	TRACE;
}

int intel_init_bsd_ring_buffer(struct drm_device *dev)
{
	TRACE;
	return 0;
}

int intel_init_blt_ring_buffer(struct drm_device *dev)
{
	TRACE;
	return 0;
}

int intel_init_vebox_ring_buffer(struct drm_device *dev)
{
	TRACE;
	return 0;
}

int __must_check i915_gem_context_init(struct drm_device *dev)
{
	TRACE;
	return 0;
}

void spin_lock_irq(spinlock_t *lock)
{
	TRACE;
}

void spin_unlock_irq(spinlock_t *lock)
{
	TRACE;
}

int fb_get_options(const char *name, char **option)
{
	using namespace Genode;

	String<64> con_to_scan(name);

	/* try to read custom user config */
	try {
		config()->reload();
		Xml_node node = config()->xml_node();
		Xml_node xn = node.sub_node();
		for (unsigned i = 0; i < node.num_sub_nodes(); xn = xn.next()) {
			if (!xn.has_type("connector")) continue;

			String<64> con_policy;
			xn.attribute("name").value(&con_policy);
			if (!(con_policy == con_to_scan)) continue;

			bool enabled = xn.attribute_value("enabled", true);
			if (!enabled) {
				*option = (char*)"d";
				return 0;
			}

			unsigned width, height;
			xn.attribute("width").value(&width);
			xn.attribute("height").value(&height);

			*option = (char*)kmalloc(64, GFP_KERNEL);
			Genode::snprintf(*option, 64, "%ux%u", width, height);
			PLOG("set connector %s to %ux%u", con_policy.string(), width, height);
		}
	} catch (...) { }
	return 0;
}

void vga_switcheroo_client_fb_set(struct pci_dev *dev, struct fb_info *info)
{
	TRACE;
}


int atomic_notifier_chain_register(struct atomic_notifier_head *nh, struct notifier_block *nb)
{
	TRACE;
	return 0;
}

int register_sysrq_key(int key, struct sysrq_key_op *op)
{
	TRACE;
	return 0;
}

void drm_vblank_off(struct drm_device *dev, int crtc)
{
	TRACE;
}


void hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize, char *linebuf, size_t linebuflen, bool ascii)
{
	TRACE;
}

void trace_intel_gpu_freq_change(int)
{
	TRACE;
}

struct cpufreq_policy *cpufreq_cpu_get(unsigned int cpu)
{
	TRACE;
	return NULL;
}

int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh, struct notifier_block *nb)
{
	TRACE;
	return 0;
}

int unregister_sysrq_key(int key, struct sysrq_key_op *op)
{
	TRACE;
	return 0;
}

void drm_gem_object_unreference_unlocked(struct drm_gem_object *obj)
{
	TRACE;
}

DEFINE_SPINLOCK(mchdev_lock);
