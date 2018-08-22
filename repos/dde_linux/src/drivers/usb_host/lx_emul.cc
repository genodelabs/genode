/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2012-01-29
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <timer_session/connection.h>
#include <util/bit_allocator.h>
#include <util/string.h>
#include <os/backtrace.h>

/* Local includes */
#include "signal.h"
#include "lx_emul.h"

#include <lx_kit/backend_alloc.h>
#include <lx_kit/irq.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/work.h>


#include <lx_emul/impl/slab.h>
#include <lx_emul/impl/mutex.h>

namespace Genode {
	class Slab_backend_alloc;
	class Slab_alloc;
}


unsigned long jiffies;
void lx_backtrace() { Genode::backtrace(); }


void pci_dev_put(struct pci_dev *pci_dev)
{
	Genode::destroy(&Lx_kit::env().heap(), pci_dev);
}


/*************************************
 ** Memory allocation, linux/slab.h **
 *************************************/

void *dma_malloc(size_t size)
{
	return Lx::Malloc::dma().alloc_large(size);
}


void dma_free(void *ptr)
{
	Lx::Malloc::dma().free_large(ptr);
}


void vfree(void *addr)
{
	if (!addr) return;
	Lx::Malloc::mem().free_large(addr);
}


/******************
 ** linux/kref.h **
 ******************/

void kref_init(struct kref *kref)
{ 
	lx_log(DEBUG_KREF,"%s ref: %p", __func__, kref);
	atomic_set(&kref->refcount, 1);
}


void kref_get(struct kref *kref)
{
	atomic_inc(&kref->refcount);
	lx_log(DEBUG_KREF, "%s ref: %p c: %d", __func__, kref, kref->refcount.counter);
}


int  kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	lx_log(DEBUG_KREF, "%s: ref: %p c: %d", __func__, kref, kref->refcount.counter);

	if(!atomic_dec_return(&kref->refcount)) {
		release(kref);
		return 1;
	}
	return 0;
}


/*********************
 ** linux/uaccess.h **
 *********************/

size_t copy_to_user(void *dst, void const *src, size_t len)
{
	if (dst && src && len)
		memcpy(dst, src, len);
	return 0;
}


bool access_ok(int access, void *addr, size_t size) { return 1; }


int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	Genode::String_console sc(buf, size);
	sc.vprintf(fmt, args);

	return sc.len();
}


int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Genode::String_console sc(buf, size);
	sc.vprintf(fmt, args);
	va_end(args);

	return sc.len();
}


int scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Genode::String_console sc(buf, size);
	sc.vprintf(fmt, args);
	va_end(args);

	return sc.len();
}


size_t strlen(const char *s) { return Genode::strlen(s); }


/*****************
 ** linux/gfp.h **
 *****************/

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return (unsigned long)kzalloc(PAGE_SIZE, 0);
}


/******************
 ** linux/log2.h **
 ******************/

int ilog2(u32 n) { return Genode::log2(n); }


/********************
 ** linux/slab.h   **
 ********************/


void *kmem_cache_zalloc(struct kmem_cache *cache, gfp_t flags)
{
	void *ret;
	ret = kmem_cache_alloc(cache, flags);
	memset(ret, 0, cache->size());

	return ret;
}


/********************
 ** linux/device.h **
 ********************/

/**
 * Simple driver management class
 */
class Driver : public Genode::List<Driver>::Element
{
	private:

		struct device_driver *_drv; /* Linux driver */

	public:

		Driver(struct device_driver *drv) : _drv(drv)
		{
			list()->insert(this);
		}

		/**
		 * List of all currently registered drivers
		 */
		static Genode::List<Driver> *list()
		{
			static Genode::List<Driver> _list;
			return &_list;
		}

		/**
		 * Match device and drivers
		 */
		bool match(struct device *dev)
		{
			/*
			 *  Don't try if buses don't match, since drivers often use 'container_of'
			 *  which might cast the device to non-matching type
			 */
			if (_drv->bus != dev->bus)
				return false;

			bool ret = _drv->bus->match ? _drv->bus->match(dev, _drv) : true;
			lx_log(DEBUG_DRIVER, "MATCH: %s ret: %u match: %p %p",
			            _drv->name, ret,  _drv->bus->match, _drv->probe);
			return ret;
		}

		/**
		 * Probe device with driver
		 */
		int probe(struct device *dev)
		{
			dev->driver = _drv;

			if (dev->bus->probe)
				return dev->bus->probe(dev);
			else if (_drv->probe)
				return _drv->probe(dev);

			return 0;
		}
};


int driver_register(struct device_driver *drv)
{
	lx_log(DEBUG_DRIVER, "%s at %p", drv->name, drv);
	new (Lx::Malloc::mem()) Driver(drv);
	return 0;
}


int device_add(struct device *dev)
{
	if (dev->driver)
		return 0;

	/* foreach driver match and probe device */
	for (Driver *driver = Driver::list()->first(); driver; driver = driver->next())
		if (driver->match(dev)) {
			int ret = driver->probe(dev);
			lx_log(DEBUG_DRIVER, "Probe return %d", ret);

			if (!ret)
				return 0;
		}

	return 0;
}


void device_del(struct device *dev)
{
	if (dev->driver && dev->driver->remove)
		dev->driver->remove(dev);

	if (dev->bus && dev->bus->remove)
		dev->bus->remove(dev);
}


int device_register(struct device *dev)
{
	return device_add(dev);
}


void device_unregister(struct device *dev)
{
	device_del(dev);
	put_device(dev);
}


int device_is_registered(struct device *dev)
{
	return 1;
}


void device_release_driver(struct device *dev)
{
	/* is usb_unbind_interface(dev); */
	if (dev->driver->remove)
		dev->driver->remove(dev);

	dev->driver = nullptr;
}


void put_device(struct device *dev)
{
	if (dev->ref) {
		dev->ref--;
		return;
	}

	if (dev->release)
		dev->release(dev);
	else if (dev->type && dev->type->release)
		dev->type->release(dev);
}


struct device *get_device(struct device *dev)
{
	dev->ref++;

	return dev;
}


void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}


int dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data; return 0;
}


const char *dev_name(const struct device *dev) { return dev->name; }


void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return kzalloc(size, gfp);
}


void *dev_get_platdata(const struct device *dev)
{
	return (void *)dev->platform_data;
}


void put_unaligned_le16(u16 val, void *p)
{
	struct __una_u16 *ptr = (struct __una_u16 *)p;
	ptr->x = val;
}

u32 get_unaligned_le32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}


void put_unaligned_le32(u32 val, void *p)
{
	struct __una_u32 *ptr = (struct __una_u32 *)p;
	ptr->x = val;
}


u64 get_unaligned_le64(const void *p)
{
	const struct __una_u64 *ptr = (const struct __una_u64 *)p;
	return ptr->x;
}


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

int fls(int x)
{
	if (!x)
		return 0;

	for (int i = 31; i >= 0; i--)
		if (x & (1 << i))
			return i + 1;

	return 0;
}


/*******************
 ** linux/delay.h **
 *******************/

#include <lx_emul/impl/delay.h>

void usleep_range(unsigned long min, unsigned long max)
{
	udelay(min);
}


/*********
 ** DMA **
 *********/

struct dma_pool
{
	size_t size;
	int    align;
};

struct dma_pool *dma_pool_create(const char *name, struct device *d, size_t size,
                                 size_t align, size_t alloc)
{
	lx_log(DEBUG_DMA, "size: %zx align:%zx %p", size, align, __builtin_return_address((0)));

	if (align & (align - 1))
		return 0;

	dma_pool *pool = new(Lx::Malloc::mem()) dma_pool;
	pool->align    = Genode::log2((int)align);
	pool->size     = size;
	return pool;
}


void  dma_pool_destroy(struct dma_pool *d)
{
	lx_log(DEBUG_DMA, "close");
	destroy(Lx::Malloc::mem(), d);
}


void *dma_pool_alloc(struct dma_pool *d, gfp_t f, dma_addr_t *dma)
{
	void *addr;
	addr = dma_alloc_coherent(0, d->size, dma, 0);

	lx_log(DEBUG_DMA, "addr: %p size %zx align %x phys: %lx pool %p",
	            addr, d->size, d->align, *dma, d);
	return addr;
}


void *dma_pool_zalloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
	void * ret = dma_pool_alloc(pool, mem_flags, handle);
	if (ret) Genode::memset(ret, 0, pool->size);
	return ret;
}


void *dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
	void * ret = dma_alloc_coherent(dev, size, dma_handle, flag);
	if (ret) Genode::memset(ret, 0, size);
	return ret;
}


void  dma_pool_free(struct dma_pool *d, void *vaddr, dma_addr_t a)
{
	lx_log(DEBUG_DMA, "free: addr %p, size: %zx", vaddr, d->size);
	Lx::Malloc::dma().free(vaddr);
}


void *dma_alloc_coherent(struct device *, size_t size, dma_addr_t *dma, gfp_t)
{
	void *addr = Lx::Malloc::dma().alloc(size, PAGE_SHIFT, dma);

	if (!addr)
		return 0;

	lx_log(DEBUG_DMA, "DMA pool alloc addr: %p size %zx align: %d, phys: %lx",
	            addr, size, PAGE_SHIFT, *dma);
	return addr;
}


void dma_free_coherent(struct device *, size_t size, void *vaddr, dma_addr_t)
{
	lx_log(DEBUG_DMA, "free: addr %p, size: %zx", vaddr, size);
	Lx::Malloc::dma().free(vaddr);
}


/*************************
 ** linux/dma-mapping.h **
 *************************/

dma_addr_t dma_map_single_attrs(struct device *dev, void *ptr,
                                size_t size,
                                enum dma_data_direction dir,
                                struct dma_attrs *attrs)
{
	dma_addr_t phys = (dma_addr_t)Lx::Malloc::dma().phys_addr(ptr);

	if (phys == ~0UL)
		Genode::error("translation virt->phys ", ptr, "->", Genode::Hex(phys), "failed, return ip ",
		     __builtin_return_address(0));

	lx_log(DEBUG_DMA, "virt: %p phys: %lx", ptr, phys);
	return phys;
}


dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        size_t offset, size_t size,
                        enum dma_data_direction dir)
{
	lx_log(DEBUG_DMA, "virt: %p phys: %lx offs: %zx", page->virt, page->phys, offset);
	return page->phys + offset;
}


int dma_map_sg_attrs(struct device *dev, struct scatterlist *sg,
                     int nents, enum dma_data_direction dir,
                     struct dma_attrs *attrs) { return nents; }


/*************************
 ** linux/scatterlist.h **
 *************************/

struct scatterlist *sg_next(struct scatterlist *sg)
{
	if (sg->last)
		return 0;

	return sg++;
}


struct page *sg_page(struct scatterlist *sg)
{
	if (!sg)
		return 0;
	
	return (page *)sg->page_link;
}


/*****************
 ** linux/smp.h **
 *****************/

int smp_call_function_single(int cpu, smp_call_func_t func, void *info,
                             int wait) { func(info); return 0; }


/******************
 ** linux/log2.h **
 ******************/


int rounddown_pow_of_two(u32 n)
{
	return 1U << Genode::log2(n);
}


/*****************
 ** linux/nls.h **
 *****************/

int utf16s_to_utf8s(const wchar_t *pwcs, int len,
                    enum utf16_endian endian, u8 *s, int maxlen)
{
	/*
	 * We do not convert to char, we simply copy the UTF16 plane 0 values
	 */
	u16 *out = (u16 *)s;
	u16 *in  = (u16 *)pwcs;
	int length = Genode::min(len, maxlen / 2);
	for (int i = 0; i < length; i++)
		out[i] = in[i];

	return 2 * length;
}

/**********************
 ** linux/notifier.h **
 **********************/

int raw_notifier_chain_register(struct raw_notifier_head *nh,
                                struct notifier_block *n)
{
	struct notifier_block *nl = nh->head;
	struct notifier_block *pr = 0;
	while (nl) {
		if (n->priority > nl->priority)
			break;
		pr = nl;
		nl = nl->next;
	}

	n->next = nl;
	if (pr)
		pr->next = n;
	else
		nh->head = n;

	return 0;
}


int raw_notifier_call_chain(struct raw_notifier_head *nh,
                            unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;
	struct notifier_block *nb = nh->head;

	while (nb) {

		ret = nb->notifier_call(nb, val, v);
		if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
			break;

		nb = nb->next;
	}

	return ret;
}


int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                     struct notifier_block *n)
{
	return raw_notifier_chain_register((struct raw_notifier_head *)nh, n);
}


int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v)
{
	return raw_notifier_call_chain((struct raw_notifier_head *)nh, val, v);
}


/*******************
 ** linux/timer.h **
 *******************/

#include <lx_emul/impl/timer.h>
#include <lx_emul/impl/sched.h>

signed long schedule_timeout_uninterruptible(signed long timeout)
{
	lx_log(DEBUG_COMPLETION, "%ld\n", timeout);
	schedule_timeout(timeout);
	return 0;
}


/************************
 ** linux/completion.h **
 ************************/

#include <lx_emul/impl/completion.h>


long __wait_completion(struct completion *work, unsigned long timeout)
{
	Lx::timer_update_jiffies();
	struct process_timer timer { *Lx::scheduler().current() };
	unsigned long        expire = timeout + jiffies;

	if (timeout) {
		timer_setup(&timer.timer, process_timeout, 0);
		mod_timer(&timer.timer, expire);
	}

	while (!work->done) {

		if (timeout && expire <= jiffies) return 0;

		Lx::Task * task = Lx::scheduler().current();
		work->task = (void *)task;
		task->block_and_schedule();
	}

	if (timeout) del_timer(&timer.timer);

	work->done = 0;
	return (expire > jiffies) ? (expire - jiffies) : 1;
}


/***********************
 ** linux/workqueue.h **
 ***********************/

#include <lx_emul/impl/work.h>


void tasklet_init(struct tasklet_struct *t, void (*f)(unsigned long), unsigned long d)
{
	t->func    = f;
	t->data    = d;
}


void tasklet_schedule(struct tasklet_struct *tasklet)
{
	Lx::Work *lx_work = (Lx::Work *)tasklet_wq->task;
	lx_work->schedule_tasklet(tasklet);
	lx_work->unblock();
}


void tasklet_hi_schedule(struct tasklet_struct *tasklet)
{
	tasklet_schedule(tasklet);
}


struct workqueue_struct *create_singlethread_workqueue(char const *name)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	Lx::Work *work = Lx::Work::alloc_work_queue(&Lx::Malloc::mem(), name);
	wq->task       = (void *)work;

	return wq;
}


struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...)
{
	return create_singlethread_workqueue(fmt);
}


/******************
 ** linux/wait.h **
 ******************/

#include <lx_emul/impl/wait.h>


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

	if (!(id >= start)) BUG();
	return id;
}


int object_is_on_stack(const void *obj)
{
	Genode::Thread::Stack_info info = Genode::Thread::mystack();
	if ((Genode::addr_t)obj <= info.top &&
	    (Genode::addr_t)obj >= info.base) return 1;
	return 0;
}


int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	return dev->irq;
}


static int platform_match(struct device *dev, struct device_driver *drv)
{
	if (!dev->name)
		return 0;


	printk("MATCH %s %s\n", dev->name, drv->name);
	return (Genode::strcmp(dev->name, drv->name) == 0);
}


static int platform_drv_probe(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	return drv->probe(dev);
}


struct bus_type platform_bus_type = {
	.name  = "platform",
};


int platform_driver_register(struct platform_driver *drv)
{
	/* init plarform_bus_type */
	platform_bus_type.match = platform_match;
	platform_bus_type.probe = platform_drv_probe;

	drv->driver.bus = &platform_bus_type;
	if (drv->probe)
		drv->driver.probe = platform_drv_probe;

	printk("Register: %s\n", drv->driver.name);
	return driver_register(&drv->driver);
}


struct resource *platform_get_resource(struct platform_device *dev,
                                       unsigned int type, unsigned int num)
{
	unsigned i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

		if ((type & r->flags) && num-- == 0)
			return r;
	}

	return NULL;
}


struct resource *platform_get_resource_byname(struct platform_device *dev,
                                              unsigned int type,
                                              const char *name)
{
	unsigned i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

	if (type == r->flags && !Genode::strcmp(r->name, name))
		return r;
	}

	return NULL;
}


int platform_get_irq_byname(struct platform_device *dev, const char *name)
{
	struct resource *r = platform_get_resource_byname(dev, IORESOURCE_IRQ, name);
	return r ? r->start : -1;
}


int platform_get_irq(struct platform_device *dev, unsigned int num)
{
	struct resource *r = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	return r ? r->start : -1;
}


int platform_device_register(struct platform_device *pdev)
{
	pdev->dev.bus  = &platform_bus_type;
	pdev->dev.name = pdev->name;
	/*Set parent to ourselfs */
	if (!pdev->dev.parent)
		pdev->dev.parent = &pdev->dev;
	device_add(&pdev->dev);
	return 0;
}


struct platform_device *platform_device_alloc(const char *name, int id)
{
	platform_device *pdev = (platform_device *)kzalloc(sizeof(struct platform_device), GFP_KERNEL);

	if (!pdev)
		return 0;

	int len    = strlen(name);
	pdev->name = (char *)kzalloc(len + 1, GFP_KERNEL);

	if (!pdev->name) {
		kfree(pdev);
		return 0;
	}

	memcpy(pdev->name, name, len);
	pdev->name[len] = 0;
	pdev->id = id;
	pdev->dev.dma_mask = (u64*)kzalloc(sizeof(u64),  GFP_KERNEL);

	return pdev;
}


int platform_device_add_data(struct platform_device *pdev, const void *data,
                             size_t size)
{
	void *d = NULL;

	if (data && !(d = kmemdup(data, size, GFP_KERNEL)))
		return -ENOMEM;

	kfree(pdev->dev.platform_data);
	pdev->dev.platform_data = d;

	return 0;
}


int platform_device_add(struct platform_device *pdev)
{
	return platform_device_register(pdev);
}

int platform_device_add_resources(struct platform_device *pdev,
                                  const struct resource *res, unsigned int num)
{
	struct resource *r = NULL;
	
	if (res) {
		r = (resource *)kmemdup(res, sizeof(struct resource) * num, GFP_KERNEL);
		if (!r)
			return -ENOMEM;
	}

	kfree(pdev->resource);
	pdev->resource = r;
	pdev->num_resources = num;
	return 0;
}


void *platform_get_drvdata(const struct platform_device *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}


void platform_set_drvdata(struct platform_device *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}


/**********************
 ** asm-generic/io.h **
 **********************/

void *devm_ioremap_resource(struct device *dev, struct resource *res)
{
	return ioremap(res->start, res->end - res->start);
}


/****************
  ** **
  *****/
int device_property_read_string(struct device *dev, const char *propname, const char **val)
{
	if (Genode::strcmp("dr_mode", propname) == 0) {
		*val = "host";
		return 0;
	}

	if (DEBUG_DRIVER) Genode::warning("property ", propname, " not found");
	*val = 0;
	return -EINVAL;
}


/****************
 ** linux/of.h **
 ****************/

const void *of_get_property(const struct device_node *node, const char *name, int *lenp)
{
	for (property * p = node ? node->properties : nullptr; p; p = p->next)
		if (Genode::strcmp(name, p->name) == 0) return p->value;

	if (DEBUG_DRIVER) Genode::warning("OF property ", name, " not found");
	return nullptr;
}


struct property *of_find_property(const struct device_node *np, const char *name, int *lenp)
{
	if (Genode::strcmp("non-zero-ttctrl-ttha", name) == 0) return (property*) 0xdeadbeef;

	if (DEBUG_DRIVER) Genode::warning("Could not find property ", name);
	return nullptr;
}


struct platform_device *of_find_device_by_node(struct device_node *np)
{
	return container_of(np->dev, struct platform_device, dev);
}


const struct of_device_id *of_match_device(const struct of_device_id *matches,
                                           const struct device *dev)
{
	const char * compatible = (const char*) of_get_property(dev->of_node, "compatible", 0);
	for (; matches && matches->compatible; matches++)
		if (Genode::strcmp(matches->compatible, compatible) == 0)
			return matches;
	return nullptr;
}


int of_parse_phandle_with_args(struct device_node *np,
                               const char *list_name,
                               const char *cells_name,
                               int index, struct of_phandle_args *out_args)
{
	out_args->np      = (device_node*) of_get_property(np, "fsl,usbmisc", 0);
	out_args->args[0] = 1;

	return 0;
}


int match_string(const char * const *array, size_t n, const char *string)
{
	for (size_t i = 0; i < n; i++)
		if (Genode::strcmp(string, array[i]) == 0) return i;
	return -1;
}


int strcmp(const char *a,const char *b)
{
	return Genode::strcmp(a, b);
}

struct regmap *syscon_regmap_lookup_by_phandle(struct device_node *np, const char *property) {
	return (regmap*)of_get_property(np, property, 0); }


bool of_property_read_bool(const struct device_node *np, const char *propname)
{
	if (DEBUG_DRIVER) Genode::warning("Could not find bool property ", propname);
	return false;
}


static usb_phy * __devm_usb_phy = nullptr;

struct usb_phy *devm_usb_get_phy_by_phandle(struct device *dev,
                                            const char *phandle, u8 index) {
	return __devm_usb_phy; }


int usb_add_phy_dev(struct usb_phy *phy) { __devm_usb_phy = phy; return 0; }


int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value)
{
	if (DEBUG_DRIVER) Genode::warning("Could not find property ", propname);
	return -EINVAL;
}
