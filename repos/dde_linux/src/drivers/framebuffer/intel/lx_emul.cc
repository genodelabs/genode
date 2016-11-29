/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2015-08-19
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/bit_allocator.h>
#include <base/log.h>
#include <os/attached_io_mem_dataspace.h>
#include <os/reporter.h>

/* local includes */
#include <component.h>

/* DRM-specific includes */
#include <lx_emul.h>
#include <lx_emul_c.h>
#include <lx_emul/extern_c_begin.h>
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <lx_emul/extern_c_end.h>

#include <lx_emul/impl/kernel.h>
#include <lx_emul/impl/delay.h>
#include <lx_emul/impl/slab.h>
#include <lx_emul/impl/gfp.h>
#include <lx_emul/impl/io.h>
#include <lx_emul/impl/pci.h>
#include <lx_emul/impl/work.h>
#include <lx_emul/impl/spinlock.h>
#include <lx_emul/impl/mutex.h>
#include <lx_emul/impl/sched.h>
#include <lx_emul/impl/timer.h>
#include <lx_emul/impl/completion.h>
#include <lx_emul/impl/wait.h>

static struct drm_device * lx_drm_device = nullptr;


struct Drm_guard
{
	drm_device * _dev;

	Drm_guard(drm_device *dev) : _dev(dev)
	{
		if (dev) {
			mutex_lock(&dev->mode_config.mutex);
			mutex_lock(&dev->mode_config.blob_lock);
			drm_modeset_lock_all(dev);
		}
	}

	~Drm_guard()
	{
		if (_dev) {
			drm_modeset_unlock_all(_dev);
			mutex_unlock(&_dev->mode_config.mutex);
			mutex_unlock(&_dev->mode_config.blob_lock);
		}
	}
};


template <typename FUNCTOR>
static inline void lx_for_each_connector(drm_device * dev, FUNCTOR f)
{
	struct drm_connector *connector;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		f(connector);
}


drm_display_mode *
Framebuffer::Driver::_preferred_mode(drm_connector *connector)
{
	using namespace Genode;
	using Genode::size_t;

	/* try to read configuration for connector */
	try {
		Xml_node config = _session.config();
		Xml_node xn = config.sub_node();
		for (unsigned i = 0; i < config.num_sub_nodes(); xn = xn.next()) {
			if (!xn.has_type("connector"))
				continue;

			String<64> con_policy;
			xn.attribute("name").value(&con_policy);
			if (Genode::strcmp(con_policy.string(), connector->name) != 0)
				continue;

			bool enabled = xn.attribute_value("enabled", true);
			if (!enabled)
				return nullptr;

			unsigned long width  = 0;
			unsigned long height = 0;
			xn.attribute("width").value(&width);
			xn.attribute("height").value(&height);

			struct drm_display_mode *mode;
			list_for_each_entry(mode, &connector->modes, head) {
			if (mode->hdisplay == (int) width &&
				mode->vdisplay == (int) height)
				return mode;
		};
		}
	} catch (...) {
		/**
		 * If no config is given, we take the most wide mode of a
		 * connector as long as it is connected at all
		 */
		if (connector->status != connector_status_connected)
			return nullptr;

		struct drm_display_mode *mode = nullptr, *tmp;
		list_for_each_entry(tmp, &connector->modes, head) {
			if (!mode || tmp->hdisplay > mode->hdisplay) mode = tmp;
		};
		return mode;
   	}
	return nullptr;
}


void Framebuffer::Driver::finish_initialization()
{
	lx_c_set_driver(lx_drm_device, (void*)this);
	generate_report();
	_session.config_changed();
}


#include <lx_kit/irq.h>

void Framebuffer::Driver::_poll()
{
	Lx::Pci_dev * pci_dev = (Lx::Pci_dev*) lx_drm_device->pdev->bus;
	Lx::Irq::irq().inject_irq(pci_dev->client());
}


void Framebuffer::Driver::set_polling(unsigned long poll)
{
	if (poll == _poll_ms) return;

	_poll_ms = poll;

	if (_poll_ms) {
		_timer.sigh(_poll_handler);
		_timer.trigger_periodic(_poll_ms * 1000);
	} else {
		_timer.sigh(Genode::Signal_context_capability());
	}
}


void Framebuffer::Driver::update_mode()
{
	using namespace Genode;

	Configuration old = _config;
	_config = Configuration();

	lx_for_each_connector(lx_drm_device, [&] (drm_connector *c) {
		drm_display_mode * mode = _preferred_mode(c);
		if (!mode) return;
		if (mode->hdisplay > _config._lx.width)  _config._lx.width  = mode->hdisplay;
		if (mode->vdisplay > _config._lx.height) _config._lx.height = mode->vdisplay;
	});

	lx_c_allocate_framebuffer(lx_drm_device, &_config._lx);

	{
		Drm_guard guard(lx_drm_device);
		lx_for_each_connector(lx_drm_device, [&] (drm_connector *c) {
		                       lx_c_set_mode(lx_drm_device, c, _config._lx.lx_fb,
		                                      _preferred_mode(c)); });
	}

	if (old._lx.addr)  Lx::iounmap(old._lx.addr);
	/* drm_crtc.h in drm_framebuffer_funcs definition: use drm_fb_remove */
	if (old._lx.lx_fb) drm_framebuffer_remove(old._lx.lx_fb);
}


void Framebuffer::Driver::generate_report()
{
	Drm_guard guard(lx_drm_device);

	/* detect mode information per connector */
	{
		struct drm_connector *c;
		list_for_each_entry(c, &lx_drm_device->mode_config.connector_list,
		                    head)
		{
			/*
			 * All states unequal to disconnected are handled as connected,
			 * since some displays stay in unknown state if not fill_modes()
			 * is called at least one time.
			 */
			bool connected = c->status != connector_status_disconnected;
			if ((connected && list_empty(&c->modes)) ||
			    (!connected && !list_empty(&c->modes)))
				c->funcs->fill_modes(c, 0, 0);
		}
	}

	/* check for report configuration option */
	static Genode::Reporter reporter("connectors");
	try {
		reporter.enabled(_session.config().sub_node("report")
		                 .attribute_value(reporter.name().string(), false));
	} catch (...) {
		reporter.enabled(false);
	}
	if (!reporter.is_enabled()) return;

	/* write new report */
	try {
		Genode::Reporter::Xml_generator xml(reporter, [&] ()
		{
			struct drm_connector *c;
			list_for_each_entry(c, &lx_drm_device->mode_config.connector_list,
			                    head) {
				xml.node("connector", [&] ()
				{
					bool connected = c->status == connector_status_connected;
					xml.attribute("name", c->name);
					xml.attribute("connected", connected);

					if (!connected) return;

					struct drm_display_mode *mode;
					list_for_each_entry(mode, &c->modes, head) {
						xml.node("mode", [&] ()
						{
							xml.attribute("width",  mode->hdisplay);
							xml.attribute("height", mode->vdisplay);
							xml.attribute("hz",     mode->vrefresh);
						});
					}
				});
			}
		});
	} catch (...) {
		Genode::warning("Failed to generate report");
	}
}


extern "C" {

/**********************
 ** Global variables **
 **********************/

struct task_struct *current;

struct boot_cpu_data boot_cpu_data;

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

size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}

int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name)
{
	TRACE;
	return 0;
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
 ** linux/timer.h **
 *******************/

int mod_timer_pinned(struct timer_list *timer, unsigned long expires)
{
	return mod_timer(timer, expires);
}


/*******************
 ** Kernel memory **
 *******************/

dma_addr_t page_to_phys(struct page *page)
{
	return page->paddr;
}

void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags)
{
	return kmem_cache_alloc(k, flags | __GFP_ZERO);
}

void *krealloc(const void *p, size_t size, gfp_t flags)
{
	/* use const-less version from <impl/slab.h> */
	return krealloc(const_cast<void*>(p), size, flags);
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
	if (id == 0) id = id_allocator.alloc(); /* do not use id zero */
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

void *idr_replace(struct idr *idp, void *ptr, int id)
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
			Lx::Pci_dev *dev = new (Lx::Malloc::mem()) Lx::Pci_dev(cap);
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
			pci_dev = new (Lx::Malloc::mem()) Lx::Pci_dev(cap);
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
                           resource_size_t (*)(void *, const struct resource *,
                                               resource_size_t, resource_size_t),
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
		Genode::error("io_mapping_create_wc unexpectedly called twice");
		return 0;
	}

	io_mapping *mapping = new (Lx::Malloc::mem()) io_mapping(base, size);
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

struct workqueue_struct *create_singlethread_workqueue(char const *)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	return wq;
}

struct workqueue_struct *alloc_ordered_workqueue(char const *name , unsigned int flags, ...)
{
	return create_singlethread_workqueue(name);
}

bool flush_work(struct work_struct *work)
{
	cancel_work_sync(work);
	return 0;
}



/***************
 ** Execution **
 ***************/

bool in_atomic()
{
	return false;
}


bool irqs_disabled()
{
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

unsigned int drm_debug = 0x0;


extern "C" int drm_pci_init(struct drm_driver *driver, struct pci_driver *pdriver)
{
	return pci_register_driver(pdriver);
}


void drm_ut_debug_printk(const char *function_name, const char *format, ...)
{
	using namespace Genode;

	va_list list;
	va_start(list, format);
	lx_printf("[drm:%s] ", function_name);
	lx_vprintf(format, list);
	va_end(list);
}


void drm_err(const char *format, ...)
{
	using namespace Genode;

	va_list list;
	va_start(list, format);
	lx_printf("[drm:ERROR] ");
	lx_vprintf(format, list);
	va_end(list);
}


struct drm_device *drm_dev_alloc(struct drm_driver *driver, struct device *parent)
{
	struct drm_device *dev = (drm_device *)kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return nullptr;

	dev->dev = parent;
	dev->driver = driver;

	INIT_LIST_HEAD(&dev->filelist);
	INIT_LIST_HEAD(&dev->ctxlist);
	INIT_LIST_HEAD(&dev->vmalist);
	INIT_LIST_HEAD(&dev->maplist);
	INIT_LIST_HEAD(&dev->vblank_event_list);

	spin_lock_init(&dev->buf_lock);
	spin_lock_init(&dev->event_lock);

	mutex_init(&dev->ctxlist_mutex);
	mutex_init(&dev->master_mutex);
	mutex_init(&dev->struct_mutex);

	return dev;
}


static void drm_get_minor(struct drm_device *dev, struct drm_minor **minor, int type)
{
	struct drm_minor *new_minor = (struct drm_minor*)
		kzalloc(sizeof(struct drm_minor), GFP_KERNEL);
	ASSERT(new_minor);
	new_minor->type = type;
	new_minor->dev = dev;
	*minor = new_minor;
}


int drm_dev_register(struct drm_device *dev, unsigned long flags)
{
	drm_get_minor(dev, &dev->primary, DRM_MINOR_LEGACY);

	ASSERT(!lx_drm_device);
	lx_drm_device = dev;
	return dev->driver->load(dev, flags);
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

	dev->pdev = pdev;

	pci_set_drvdata(pdev, dev);

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


int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                const char *name, void *dev)
{
	struct drm_device * drm_dev = (struct drm_device*) dev;
	Lx::Pci_dev * pci_dev = (Lx::Pci_dev*) drm_dev->pdev->bus;
	Lx::Irq::irq().request_irq(pci_dev->client(), handler, dev);
	return 0;
}

void drm_gem_private_object_init(struct drm_device *dev,
                                 struct drm_gem_object *obj, size_t size)
{
	obj->dev = dev;
	kref_init(&obj->refcount);
	obj->filp = NULL;
	obj->size = size;
}


void drm_gem_object_free(struct kref *kref)
{
	struct drm_gem_object *obj =
		container_of(kref, struct drm_gem_object, refcount);
	struct drm_device *dev = obj->dev;

	if (dev->driver->gem_free_object != NULL)
		dev->driver->gem_free_object(obj);
}


void drm_gem_free_mmap_offset(struct drm_gem_object *obj)
{
	if (obj && obj->filp && obj->filp->f_inode && obj->filp->f_inode->i_mapping)
		free_pages((unsigned long)obj->filp->f_inode->i_mapping->my_page->addr, 0);
}


void drm_gem_object_release(struct drm_gem_object *obj)
{
	if (!obj || !obj->filp || !obj->filp->f_inode
	    || !obj->filp->f_inode->i_mapping) return;

	kfree(obj->filp->f_inode->i_mapping);
	kfree(obj->filp->f_inode);
	kfree(obj->filp);
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

bool flush_delayed_work(struct delayed_work *dwork)
{
	TRACE;
	return false;
}

async_cookie_t async_schedule(async_func_t func, void *data)
{
	TRACE;
	return 0;
}

void async_synchronize_full(void)
{
	TRACE;
}


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

	Genode::memset(table, 0, sizeof(*table));

	struct scatterlist *sg = (scatterlist*)
		Lx::Malloc::mem().alloc_large(nents * sizeof(struct scatterlist));
	if (!sg) return -ENOMEM;

	Genode::memset(sg, 0, sizeof(*sg) * nents);
	table->nents = nents;
	table->sgl = sg;
	sg_mark_end(&sg[nents - 1]);
	return 0;
}


void sg_free_table(struct sg_table *table)
{
	TRACE;
	if (table && table->sgl) Lx::Malloc::mem().free_large(table->sgl);
}


static inline bool sg_is_last(scatterlist * sg) {
	return (sg->page_link & 0x02); }

struct scatterlist *sg_next(struct scatterlist * sg)
{
	if (sg_is_last(sg))
		return NULL;

	sg++;
	if (unlikely(sg_is_chain(sg)))
		sg = sg_chain_ptr(sg);

	return sg;
}

void __sg_page_iter_start(struct sg_page_iter *piter, struct scatterlist *sglist,
                          unsigned int nents, unsigned long pgoffset)
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


struct page *sg_page_iter_page(struct sg_page_iter *piter)
{
	return (page*)(PAGE_SIZE * (page_to_pfn((sg_page(piter->sg))) + (piter->sg_pgoffset)));
}


void dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	TRACE;
}


void mark_page_accessed(struct page *p)
{
	//TRACE;
}


void put_page(struct page *page)
{
	//TRACE;
}

unsigned long invalidate_mapping_pages(struct address_space *mapping, pgoff_t start, pgoff_t end)
{
	TRACE;
	return 0;
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

int kref_put_mutex(struct kref *kref, void (*release)(struct kref *kref), struct mutex *lock)
{
	if (kref_put(kref, release)) {
		mutex_lock(lock);
		return 1;
	}
	return 0;
}


void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > SIZE_MAX / size) return NULL;
	return kmalloc(n * size, flags);
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
}

int vga_client_register(struct pci_dev *pdev, void *cookie,
                        void (*irq_set_state)(void *cookie, bool state),
                        unsigned int (*set_vga_decode)(void *cookie, bool state))
{
	TRACE;
	return -ENODEV;
}

int vga_switcheroo_register_client(struct pci_dev *dev,
                                   const struct vga_switcheroo_client_ops *ops,
                                   bool driver_power_control)
{
	TRACE;
	return 0;
}

struct resource * devm_request_mem_region(struct device *dev,
                                          resource_size_t start,
                                          resource_size_t n,
                                          const char *name)
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

int drm_sysfs_connector_add(struct drm_connector *connector)
{
	TRACE;
	connector->kdev = (struct device*)
		kmalloc(sizeof(struct device), GFP_KERNEL);
	DRM_DEBUG("adding \"%s\" to sysfs\n", connector->name);
	drm_sysfs_hotplug_event(NULL);
	return 0;
}

void drm_sysfs_connector_remove(struct drm_connector *connector)
{
	TRACE;
	kfree(connector->kdev);
}

void assert_spin_locked(spinlock_t *lock)
{
}

void spin_lock_irq(spinlock_t *lock)
{
}

void spin_unlock_irq(spinlock_t *lock)
{
}

int fb_get_options(const char *name, char **option)
{
	return 0;
}

void vga_switcheroo_client_fb_set(struct pci_dev *dev, struct fb_info *info)
{
	TRACE;
}


int register_sysrq_key(int key, struct sysrq_key_op *op)
{
	TRACE;
	return 0;
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


int unregister_sysrq_key(int key, struct sysrq_key_op *op)
{
	TRACE;
	return 0;
}


int of_alias_get_highest_id(const char *stem)
{
	TRACE;
	return 0;
}

void down_write(struct rw_semaphore *sem)
{
	TRACE;
}

void up_write(struct rw_semaphore *sem)
{
	TRACE;
}

void intel_csr_ucode_init(struct drm_device *dev)
{
	TRACE;
}

void intel_guc_ucode_init(struct drm_device *dev)
{
	TRACE;
}

void i915_gem_shrinker_init(struct drm_i915_private *dev_priv)
{
	TRACE;
}

void intel_init_audio(struct drm_device *dev)
{
	TRACE;
}

bool static_key_false(struct static_key *key)
{
	TRACE;
	return false;
}

int i915_gem_init_userptr(struct drm_device *dev)
{
	TRACE;
	return 0;
}

void i915_gem_batch_pool_init(struct drm_device *dev, struct i915_gem_batch_pool *pool)
{
	TRACE;
}

void spin_lock(spinlock_t *lock)
{
}

#define __GFP_BITS_SHIFT 26
#define __GFP_BITS_MASK ((__force gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

int drm_gem_object_init(struct drm_device *dev, struct drm_gem_object *obj, size_t size)
{
	drm_gem_private_object_init(dev, obj, size);

	struct file          * filp    = (struct file*) kmalloc(sizeof(struct file), GFP_KERNEL);
	struct inode         * inode   = (struct inode*) kmalloc(sizeof(struct inode*), GFP_KERNEL);
	struct address_space * mapping = (struct address_space*)
		kmalloc(sizeof(struct address_space*), GFP_KERNEL);

	inode->i_mapping = mapping;
	filp->f_inode    = inode;
	obj->filp        = filp;

	unsigned long npages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	size_t sz_log2 = Genode::log2(npages);
	sz_log2 += (npages > (1UL << sz_log2)) ? 1 : 0;
	struct page *pages = alloc_pages(0, sz_log2);
	mapping->my_page = pages;

	size = PAGE_SIZE * npages;
	void * data = page_address(pages);
	memset(data, 0, size);

	return 0;
}

struct inode *file_inode(struct file *f)
{
	return f->f_inode;
}

void mapping_set_gfp_mask(struct address_space *m, gfp_t mask)
{
	TRACE;
}

gfp_t mapping_gfp_constraint(struct address_space *mapping, gfp_t gfp_mask)
{
	TRACE;
	return 0;
}

struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
                                         pgoff_t index, gfp_t gfp_mask)
{
	return mapping->my_page;
}

void sg_set_page(struct scatterlist *sg, struct page *page,
                 unsigned int len, unsigned int offset)
{
	unsigned long page_link = sg->page_link & 0x3;
	sg->page_link = page_link | (unsigned long) page;
	sg->offset = offset;
	sg->length = len;
}

dma_addr_t page_to_pfn(struct page *page)
{
	return page->paddr / PAGE_SIZE;
}

struct page *sg_page(struct scatterlist *sg)
{
	return (struct page *)((sg)->page_link & ~0x3);
}

void *sg_virt(struct scatterlist *sg)
{
	return (void*)((size_t)page_address(sg_page(sg)) + sg->offset);
}

int dma_map_sg_attrs(struct device *dev, struct scatterlist *sg, int nents,
                     enum dma_data_direction dir, struct dma_attrs *attrs)
{
	int i;
	struct scatterlist *s;
	Genode::addr_t base = page_to_phys(sg_page(sg));
	Genode::size_t offs = 0;
	for_each_sg(sg, s, nents, i) {
		s->dma_address = base + offs;
		offs += s->length;
	}
	return nents;
}

dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        unsigned long offset, size_t size,
                        enum dma_data_direction direction)
{
	return page_to_phys(page) + offset;
}

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return 0;
}

int on_each_cpu(void (*func) (void *info), void *info, int wait)
{
	func(info);
	return 0;
}

void wbinvd()
{
	TRACE;
}

int i915_cmd_parser_init_ring(struct intel_engine_cs *ring)
{
	TRACE;
	return 0;
}

u64 ktime_get_raw_ns(void)
{
	return ktime_get().tv64;
}

ktime_t ktime_add_ns(const ktime_t kt, u64 nsec)
{
	ktime_t ktime;
	ktime.tv64 = kt.tv64 + nsec;
	return ktime;
}

s64 ktime_us_delta(const ktime_t later, const ktime_t earlier)
{
	return ktime_to_us(ktime_sub(later, earlier));
}

void i915_setup_sysfs(struct drm_device *dev_priv)
{
	TRACE;
}

int acpi_video_register(void)
{
	TRACE;
	return 0;
}

void i915_audio_component_init(struct drm_i915_private *dev_priv)
{
	TRACE;
}

void ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class)
{
	lock->ctx = NULL;
	lock->locked = false;
}

void ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *ww_class)
{
	TRACE;
}

int  ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	if (ctx && (lock->ctx == ctx))
		return -EALREADY;

	lock->ctx = ctx;
	lock->locked = true;
	return 0;
}

void ww_mutex_unlock(struct ww_mutex *lock)
{
	lock->ctx = NULL;
	lock->locked = false;
}

bool ww_mutex_is_locked(struct ww_mutex *lock)
{
	return lock->locked;
}

void ww_acquire_fini(struct ww_acquire_ctx *ctx)
{
	TRACE;
}

void local_irq_disable()
{
	TRACE;
}

void local_irq_enable()
{
	TRACE;
}

void drm_sysfs_hotplug_event(struct drm_device *dev)
{
	Framebuffer::Driver * driver = (Framebuffer::Driver*)
		lx_c_get_driver(lx_drm_device);

	if (driver) {
		DRM_DEBUG("generating hotplug event\n");
		driver->generate_report();
	}
}

void intel_audio_codec_enable(struct intel_encoder *encoder)
{
	TRACE;
}

static void clflush(uint8_t * page)
{
	unsigned int i;
	const int size = 64;

	ASSERT(sizeof(unsigned long) == 8);

	// FIXME clflush with other opcode see X86_FEATURE_CLFLUSHOPT
	for (i = 0; i < PAGE_SIZE; i += size)
		asm volatile(".byte 0x3e; clflush %P0"
		             : "+m" (*(volatile char __force *)(page+i)));
}

void drm_clflush_sg(struct sg_table *st)
{
	unsigned int i;
	struct scatterlist *s;

	Genode::addr_t base = (Genode::addr_t) sg_virt(st->sgl);
	Genode::size_t offs = 0;

	asm volatile("mfence":::"memory");
	for_each_sg(st->sgl, s, st->nents, i) {
		clflush((uint8_t*)(base + offs));
		offs += s->length;
	}
	asm volatile("mfence":::"memory");
}

void intel_audio_codec_disable(struct intel_encoder *encoder)
{
	TRACE;
}

struct backlight_device *backlight_device_register(const char *name,
  struct device *dev, void *devdata, const struct backlight_ops *ops,
  const struct backlight_properties *props)
{
	struct backlight_device *new_bd;
	new_bd = (backlight_device*) kzalloc(sizeof(struct backlight_device), GFP_KERNEL);
	return new_bd;
}

void synchronize_irq(unsigned int irq)
{
	TRACE;
}

void bitmap_zero(unsigned long *dst, unsigned int nbits)
{
	unsigned int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
	Genode::memset(dst, 0, len);
}

int bitmap_empty(const unsigned long *src, unsigned nbits)
{
	return find_first_bit(src, nbits) == nbits;
}

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))
unsigned long find_next_bit(const unsigned long *addr, unsigned long nbits,
                            unsigned long start)
{
	unsigned long tmp;

	if (!nbits || start >= nbits)
		return nbits;

	tmp = addr[start / BITS_PER_LONG] ^ 0UL;

	/* Handle 1st word. */
	tmp &= BITMAP_FIRST_WORD_MASK(start);
	start = round_down(start, BITS_PER_LONG);

	while (!tmp) {
		start += BITS_PER_LONG;
		if (start >= nbits)
			return nbits;
		tmp = addr[start / BITS_PER_LONG] ^ 0UL;
	}

	return min(start + __ffs(tmp), nbits);
}

void bitmap_set(unsigned long *map, unsigned int start, int len)
{
	unsigned long *p = map + BIT_WORD(start);
	const unsigned int size = start + len;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (len - bits_to_set >= 0) {
		*p |= mask_to_set;
		len -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (len) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		*p |= mask_to_set;
	}
}

void bitmap_or(unsigned long *dst, const unsigned long *src1,
               const unsigned long *src2, unsigned int nbits)
{
	unsigned int k;
	unsigned int nr = BITS_TO_LONGS(nbits);

	for (k = 0; k < nr; k++)
		dst[k] = src1[k] | src2[k];
}

int i915_gem_render_state_init(struct drm_i915_gem_request *req)
{
	TRACE;
	return 0;
}

int intel_dp_mst_encoder_init(struct intel_digital_port *intel_dig_port, int conn_id)
{
	TRACE;
	return 0;
}

signed long schedule_timeout_uninterruptible(signed long timeout)
{
	return schedule_timeout(timeout);
}

int intel_logical_rings_init(struct drm_device *dev)
{
	TRACE;
	return 0;
}

int intel_guc_ucode_load(struct drm_device *dev)
{
	TRACE;
	return 0;
}

void intel_lrc_irq_handler(struct intel_engine_cs *ring)
{
	TRACE;
}

} /* extern "C" */
