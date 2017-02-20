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
#include <util/string.h>

/* Local includes */
#include "signal.h"
#include "lx_emul.h"

#include <lx_kit/backend_alloc.h>
#include <lx_kit/irq.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/work.h>


#include <lx_emul/impl/slab.h>

namespace Genode {
	class Slab_backend_alloc;
	class Slab_alloc;
}


unsigned long jiffies;
void backtrace() { }


void pci_dev_put(struct pci_dev *pci_dev)
{
	Genode::destroy(&Lx_kit::env().heap(), pci_dev);
}

/***********************
 ** Atomic operations **
 ***********************/

/*
 * Actually not atomic, for now
 */

unsigned int atomic_read(atomic_t *p) { return p->v; }

void atomic_inc(atomic_t *v) { v->v++;  }
void atomic_dec(atomic_t *v) { v->v--; }

void atomic_add(int i, atomic_t *v) { v->v += i; }
void atomic_sub(int i, atomic_t *v) { v->v -= i; }

void atomic_set(atomic_t *p, unsigned int v) { p->v = v; }


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


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vzalloc(unsigned long size)
{
	size_t *addr;
	try { addr = (size_t *)Lx::Malloc::mem().alloc_large(size); }
	catch (...) { return 0; }

	memset(addr, 0, size);

	return addr;
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
	kref->refcount.v = 1; 
}


void kref_get(struct kref *kref)
{
	kref->refcount.v++;
	lx_log(DEBUG_KREF, "%s ref: %p c: %d", __func__, kref, kref->refcount.v);
}


int  kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	lx_log(DEBUG_KREF, "%s: ref: %p c: %d", __func__, kref, kref->refcount.v);

	if (!--kref->refcount.v) {
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


size_t copy_from_user(void *dst, void const *src, size_t len)
{
	if (dst && src && len)
		memcpy(dst, src, len);
	return 0;
}


bool access_ok(int access, void *addr, size_t size) { return 1; }


/********************
 ** linux/string.h **
 ********************/

void *_memcpy(void *d, const void *s, size_t n)
{
	return Genode::memcpy(d, s, n);
}


inline void *memset(void *s, int c, size_t n) 
{
	return Genode::memset(s, c, n);
}


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

int    strcmp(const char *s1, const char *s2)
{
	printk("%s:%d from %p\n", __func__, __LINE__, __builtin_return_address(0));
	return Genode::strcmp(s1, s2);
}
size_t strlen(const char *s) { return Genode::strlen(s); }


size_t strlcat(char *dest, const char *src, size_t dest_size)
{
	size_t len_d = strlen(dest);
	size_t len_s = strlen(src);

	if (len_d > dest_size)
		return 0;

	size_t len = dest_size - len_d - 1;

	memcpy(dest + len_d, src, len);
	dest[len_d + len] = 0;
	return len;
}


size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		Genode::memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}


void *memscan(void *addr, int c, size_t size)
{
	unsigned char* p = (unsigned char *)addr;

	for (size_t s = 0; s < size; s++, p++)
		if (*p == c)
			break;

	return (void *)p;
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


/**********************
 ** asm-generic/io.h **
 **********************/

void *phys_to_virt(unsigned long address)
{
	return (void *)Lx::Malloc::dma().virt_addr(address);
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

			if (dev->bus->probe) {
				lx_log(DEBUG_DRIVER, "Probing device bus %p", dev->bus->probe);
				return dev->bus->probe(dev);
			} else if (_drv->probe) {
				lx_log(DEBUG_DRIVER, "Probing driver: %s %p", _drv->name, _drv->probe);
				return _drv->probe(dev);
			}

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
	lx_log(DEBUG_DRIVER, "Remove device %p", dev);
	if (dev->driver && dev->driver->remove)
		dev->driver->remove(dev);
}


int device_register(struct device *dev)
{
	//XXX: initialize DMA pools (see device_initialize)
	return device_add(dev);
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

/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
                            unsigned long offset)
{
	unsigned long i  = offset / BITS_PER_LONG;
	offset -= (i * BITS_PER_LONG);

	for (; offset < size; offset++)
		if (addr[i] & (1UL << offset))
			return offset + (i * BITS_PER_LONG);

	return size;
}


long find_next_zero_bit_le(const void *addr,
                           unsigned long size, unsigned long offset)
{
	static unsigned cnt = 0;
	unsigned long max_size = sizeof(long) * 8;
	if (offset >= max_size) {
		Genode::warning("Offset greater max size");
		return offset + size;
	}

	for (; offset < max_size; offset++)
		if (!(*(unsigned long*)addr & (1L << offset)))
			return offset;

	Genode::warning("No zero bit findable");

	return offset + size;
}


void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return kzalloc(size, gfp);
}


void *dev_get_platdata(const struct device *dev)
{
	return (void *)dev->platform_data;
}


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

u16 get_unaligned_le16(const void *p)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *)p;
	return ptr->x;
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


void put_unaligned_le64(u64 val, void *p)
{
	struct __una_u64 *ptr = (struct __una_u64 *)p;
	ptr->x = val;
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


/*********************
 ** linux/kthread.h **
 *********************/

struct task_struct *kthread_run(int (*fn)(void *), void *arg, const char *n, ...)
{
	/*
	 * This is just called for delayed device scanning (see
	 * 'drivers/usb/storage/usb.c')
	 */
	lx_log(DEBUG_THREAD, "Run %s", n);

	new (Lx::Malloc::mem()) Lx::Task((void (*)(void *))fn, arg, n,
	                                 Lx::Task::PRIORITY_2,
	                                 Lx::scheduler());
	return 0;
}


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


void *sg_virt(struct scatterlist *sg)
{
	if (!sg || !sg->page_link)
		return 0;

	struct page *page = (struct page *)sg->page_link;
	return (void *)((unsigned long)page->virt + sg->offset);
}


/********************
 ** linux/ioport.h **
 ********************/

resource_size_t resource_size(const struct resource *res)
{
	return res->end - res->start + 1;
}

struct resource * devm_request_mem_region(struct device *dev, resource_size_t start,
                                          resource_size_t n, const char *name)
{
	struct resource *r = (struct resource *)kzalloc(sizeof(struct resource), GFP_KERNEL);
	r->start = start;
	r->end   = start + n - 1;
	r->name  = name;

	return r;
}


/*****************
 ** linux/smp.h **
 *****************/

int smp_call_function_single(int cpu, smp_call_func_t func, void *info,
                             int wait) { func(info); return 0; }


/****************
 ** Networking **
 ****************/


/*************************
 ** linux/etherdevice.h **
 *************************/

struct net_device *alloc_etherdev(int sizeof_priv)
{
	net_device *dev = new (Lx::Malloc::mem()) net_device();

	dev->mtu      = 1500;
	dev->hard_header_len = 0;
	dev->priv     = kzalloc(sizeof_priv, 0);
	dev->dev_addr = dev->_dev_addr;
	memset(dev->_dev_addr, 0, sizeof(dev->_dev_addr));

	return dev;
}


int is_valid_ether_addr(const u8 *addr)
{
	/* is multicast */
	if ((addr[0] & 0x1))
		return 0;

	/* zero */
	if (!(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]))
		return 0;

	return 1;
}


/*****************
 ** linux/mii.h **
 *****************/

/**
 * Restart NWay (autonegotiation) for this interface
 */
int mii_nway_restart (struct mii_if_info *mii)
{
	int bmcr;
	int r = -EINVAL;
	enum { 
		BMCR_ANENABLE  = 0x1000, /* enable auto negotiation */
		BMCR_ANRESTART = 0x200,  /* auto negotation restart */
	};

	/* if autoneg is off, it's an error */
	bmcr = mii->mdio_read(mii->dev, mii->phy_id, MII_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		printk("Reanable\n");
		bmcr |= BMCR_ANRESTART;
		mii->mdio_write(mii->dev, mii->phy_id, MII_BMCR, bmcr);
		r = 0;
	}

	return r;
}


int mii_ethtool_gset(struct mii_if_info *mii, struct ethtool_cmd *ecmd)
{
	ecmd->duplex = DUPLEX_FULL;
	return 0;
}


u8 mii_resolve_flowctrl_fdx(u16 lcladv, u16 rmtadv)
{
	u8 cap = 0;

	if (lcladv & rmtadv & ADVERTISE_PAUSE_CAP) {
		cap = FLOW_CTRL_TX | FLOW_CTRL_RX;
	} else if (lcladv & rmtadv & ADVERTISE_PAUSE_ASYM) {
		if (lcladv & ADVERTISE_PAUSE_CAP)
			cap = FLOW_CTRL_RX;
		else if (rmtadv & ADVERTISE_PAUSE_CAP)
			cap = FLOW_CTRL_TX;
	}

	 return cap;
}

int mii_link_ok (struct mii_if_info *mii)
{
	/* first, a dummy read, needed to latch some MII phys */
	mii->mdio_read(mii->dev, mii->phy_id, MII_BMSR);
	if (mii->mdio_read(mii->dev, mii->phy_id, MII_BMSR) & BMSR_LSTATUS)
	        return 1;
	return 0;
}


unsigned int mii_check_media (struct mii_if_info *mii,
                              unsigned int ok_to_print,
                              unsigned int init_media)
{
	if (mii_link_ok(mii))
		netif_carrier_on(mii->dev);
	else
		netif_carrier_off(mii->dev);
	return 0;
}


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


static void _completion_timeout(unsigned long t)
{
	Lx::Task *task = (Lx::Task *)t;
	task->unblock();
}


long __wait_completion(struct completion *work, unsigned long timeout)
{
	timer_list t;
	unsigned long j = timeout ? jiffies + timeout : 0;

	if (timeout) {
		setup_timer(&t, _completion_timeout, (unsigned long)Lx::scheduler().current());
		mod_timer(&t, timeout);
	}

	while (!work->done) {

		if (j && j <= jiffies) {
			lx_log(1, "timeout jiffies %lu", jiffies);
			return 0;
		}

		Lx::Task *task = Lx::scheduler().current();
		work->task = (void *)task;
		task->block_and_schedule();
	}

	if (timeout)
		del_timer(&t);

	work->done = 0;

	return j ? j - jiffies : 1;
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

