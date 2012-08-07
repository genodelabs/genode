/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2012-01-29
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <timer_session/connection.h>
#include <util/string.h>

/* Local includes */
#include "mem.h"
#include "routine.h"
#include "signal.h"
#include "lx_emul.h"

/* DDE kit includes */
extern "C" {
#include <dde_kit/semaphore.h>
#include <dde_kit/memory.h>
#include <dde_kit/pgtab.h>
#include <dde_kit/resources.h>
#include <dde_kit/interrupt.h>
#include <dde_kit/thread.h>
}


#if VERBOSE_LX_EMUL
#define TRACE       dde_kit_printf("\033[35m%s\033[0m called\n",                __PRETTY_FUNCTION__)
#define UNSUPPORTED dde_kit_printf("\033[31m%s\033[0m unsupported arguments\n", __PRETTY_FUNCTION__)
#else
#define TRACE
#define UNSUPPORTED
#endif

namespace Genode {

	class Slab_alloc : public Slab
	{
		private:
			
			Mem::Zone_alloc *_allocator;

			size_t _calculate_block_size(size_t object_size)
			{
				size_t block_size = 8 * (object_size + sizeof(Slab_entry)) + sizeof(Slab_block);
				return align_addr(block_size, 12);
			}

		public:

			Slab_alloc(size_t object_size, Mem::Zone_alloc *allocator)
			: Slab(object_size, _calculate_block_size(object_size), 0, allocator),
			  _allocator(allocator) { }

		inline void *alloc()
		{
			void *result;
			return (Slab::alloc(slab_size(), &result) ? result : 0);
		}

		bool match(void const *addr) { return _allocator->match(addr); }
		addr_t phys_addr(void const *addr) { return _allocator->phys_addr(addr); }
	};
}


class Malloc
{
	private:

		Genode::Mem *_pool;

		enum { 
			SLAB_START_LOG2 = 3,  /* 8 B */
			SLAB_STOP_LOG2  = 16, /* 64 KB */
			NUM_SLABS = (SLAB_STOP_LOG2 - SLAB_START_LOG2) + 1
		};

		/* slab allocator using Mem as back-end */
		Genode::Slab_alloc *_allocator[NUM_SLABS];

		void _init_slabs()
		{
			using namespace Genode;
			_pool->init_zones(NUM_SLABS);
			for (unsigned i = SLAB_START_LOG2; i <= SLAB_STOP_LOG2; i++) {
				Mem::Zone_alloc *allocator = _pool->new_zone_allocator();
				_allocator[i - SLAB_START_LOG2] = new (env()->heap()) Slab_alloc(1U << i, allocator);
			}
		}

		/**
		 * Return slab for 'struct dma_pool' of size
		 */
		int _dma_pool_slab(Genode::size_t size)
		{
			int msb = Genode::log2(size);
			if (size > (1U << msb))
				msb++;

			/* take next chunk */
			return msb++;
		}

	public:

		Malloc(Genode::Mem *pool) : _pool(pool) { _init_slabs(); }

		/**
		 * General purpose allcator
		 */
		static Malloc *mem()
		{
			static Malloc _m(Genode::Mem::pool());
			return &_m;
		}

		/**
		 * DMA allocator
		 */
		static Malloc *dma()
		{
			static Malloc _m(Genode::Mem::dma());
			return &_m;
		}

		/**
		 * Alloc with alignment (uses back-end when alingment is > 2)
		 */
		void *alloc(Genode::size_t size, int align)
		{
			if (align <= 2)
				return alloc(size);

			return _pool->alloc(size, -1, align);
		}


		/**
		 * Alloc in slabs
		 */
		void *alloc(Genode::size_t size)
		{
			int msb = Genode::log2(size);

			if (size > (1U << msb))
				msb++;

			if (size < (1U << SLAB_START_LOG2))
				msb = SLAB_STOP_LOG2;

			if (msb > SLAB_STOP_LOG2) {
				PINF("Slab too large %u", 1U << msb);
				return _pool->alloc(size);
			}

			return _allocator[msb - SLAB_START_LOG2]->alloc();
		}


		/**
		 * Free from slabs
		 */
		void free(void const *addr)
		{

			for (register unsigned i = SLAB_START_LOG2; i <= SLAB_STOP_LOG2; i++) {
				Genode::Slab_alloc *slab = _allocator[i - SLAB_START_LOG2];
				
				if (!slab->match(addr))
					continue;

				slab->free((void *)addr);
				return;
			}
			
			_pool->free((void *)addr);
		}

		/**
		 * Get phys addr
		 */
		Genode::addr_t phys_addr(void *addr)
		{
			for (register unsigned i = SLAB_START_LOG2; i <= SLAB_STOP_LOG2; i++) {
				Genode::Slab_alloc *slab = _allocator[i - SLAB_START_LOG2];
				
				if (!slab->match(addr))
					continue;

				return slab->phys_addr(addr);
			}
			/* not found in slabs, try in back-end */
			return _pool->phys_addr(addr);
		}


		/**
		 * Allocate aligned memory in slabs
		 */
		void *dma_pool_alloc(size_t size, int align, Genode::addr_t *dma)
		{
			using namespace Genode;

			int msb = _dma_pool_slab(size);
			addr_t base = (addr_t)_allocator[msb - SLAB_START_LOG2]->alloc();

			unsigned align_val = (1U << align);
			unsigned align_mask = align_val - 1;

			/* make room for pointer */
			addr_t addr = base + sizeof(Genode::addr_t);

			/* align */
			addr = (addr + align_val - 1) & ~align_mask;
			addr_t *ptr = (addr_t *)addr - 1;
			*ptr = base;

			*dma = phys_addr((void *)addr);
			return (void *)addr;
		}

		/**
		 * Free memory allocted with 'dma_pool_alloc'
		 */
		void dma_pool_free(size_t size, void *addr)
		{
			using namespace Genode;

			int msb = _dma_pool_slab(size);
			addr_t base = *((addr_t *)addr - 1);
			_allocator[msb - SLAB_START_LOG2]->free((void *)base);
		}
};


/***********************
 ** Atomic operations **
 ***********************/

/*
 * Actually not atomic, for now
 */

unsigned int atomic_read(atomic_t *p) { return *(volatile int *)p; }

void atomic_inc(atomic_t *v) { (*(volatile int *)v)++; }
void atomic_dec(atomic_t *v) { (*(volatile int *)v)--; }

void atomic_add(int i, atomic_t *v) { (*(volatile int *)v) += i; }
void atomic_sub(int i, atomic_t *v) { (*(volatile int *)v) -= i; }

void atomic_set(atomic_t *p, unsigned int v) { (*(volatile int *)p) = v; }

/*******************
 ** linux/mutex.h **
 *******************/

void mutex_init  (struct mutex *m) { if (m->lock) dde_kit_lock_init  (&m->lock); }
void mutex_lock  (struct mutex *m) { if (m->lock) dde_kit_lock_lock  ( m->lock); }
void mutex_unlock(struct mutex *m) { if (m->lock) dde_kit_lock_unlock( m->lock); }


/*************************************
 ** Memory allocation, linux/slab.h **
 *************************************/

void *kmalloc(size_t size, gfp_t flags)
{
	void *addr = flags & GFP_NOIO ? Malloc::dma()->alloc(size) : Malloc::mem()->alloc(size);
	return addr;
}


void *kzalloc(size_t size, gfp_t flags)
{
	void *addr = kmalloc(size, flags);
	if (addr)
		Genode::memset(addr, 0, size);

	return addr;
}


void *kcalloc(size_t n, size_t size, gfp_t flags)
{
	 if (size != 0 && n > ~0UL / size)
		return 0;

	return kzalloc(n * size, flags);
}

void kfree(const void *p)
{
	Malloc::mem()->free(p);
	Malloc::dma()->free(p);
}


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vzalloc(unsigned long size) 
{
	void *ptr = dde_kit_simple_malloc(size);
	if (ptr)
		Genode::memset(ptr, 0, size);

	return ptr;
}


void vfree(void *addr) { dde_kit_simple_free(addr); }


/******************
 ** linux/kref.h **
 ******************/

void kref_init(struct kref *kref)
{ 
	dde_kit_log(DEBUG_KREF,"%s ref: %p", __func__, kref);
	kref->refcount.v = 1; 
}


void kref_get(struct kref *kref)
{
	kref->refcount.v++;
	dde_kit_log(DEBUG_KREF, "%s ref: %p c: %d", __func__, kref, kref->refcount.v);
}


int  kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	dde_kit_log(DEBUG_KREF, "%s: ref: %p c: %d", __func__, kref, kref->refcount.v);

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

int    strcmp(const char *s1, const char *s2) { return Genode::strcmp(s1, s2); }
size_t strlen(const char *s) { return Genode::strlen(s); }


size_t strlcat(char *dest, const char *src, size_t n)
{
	size_t len = strlen(dest);

	if (len >= n)
		len = n - 1;

	memcpy(dest, src, len);
	dest[len] = 0;
	return len;
}


void  *kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *ptr = kmalloc(len, 0);
	memcpy(ptr, src, len);
	return ptr;
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

struct kmem_cache
{
	const char *name;  /* cache name */
	unsigned    size;  /* object size */

	struct dde_kit_slab *dde_kit_slab_cache; /* backing DDE kit cache */
};


struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long falgs, void (*ctor)(void *))
{
	dde_kit_log(DEBUG_SLAB, "\"%s\" obj_size=%d", name, size);

	struct kmem_cache *cache;

	if (!name) {
		printk("kmem_cache name reqeuired\n");
		return 0;
	}

	cache = (struct kmem_cache *)dde_kit_simple_malloc(sizeof(*cache));
	if (!cache) {
		printk("No memory for slab cache\n");
		return 0;
	}

	/* initialize a physically contiguous cache for kmem */
	if (!(cache->dde_kit_slab_cache = dde_kit_slab_init(size))) {
		printk("DDE kit slab init failed\n");
		dde_kit_simple_free(cache);
		return 0;
	}

	cache->name = name;
	cache->size = size;

	return cache;
}


void kmem_cache_destroy(struct kmem_cache *cache)
{
	dde_kit_log(DEBUG_SLAB, "\"%s\"", cache->name);

	dde_kit_slab_destroy(cache->dde_kit_slab_cache);
	dde_kit_simple_free(cache);
}


void *kmem_cache_zalloc(struct kmem_cache *cache, gfp_t flags)
{
	void *ret;

	dde_kit_log(DEBUG_SLAB, "\"%s\" flags=%x", cache->name, flags);

	ret = dde_kit_slab_alloc(cache->dde_kit_slab_cache);

	/* return here in case of error */
	if (!ret) return 0;

	/* zero page */
	memset(ret, 0, cache->size);

	return ret;
}


void kmem_cache_free(struct kmem_cache *cache, void *objp)
{
	dde_kit_log(DEBUG_SLAB, "\"%s\" (%p)", cache->name, objp);
	dde_kit_slab_free(cache->dde_kit_slab_cache, objp);
}


/**********************
 ** asm-generic/io.h **
 **********************/

void *_ioremap(resource_size_t phys_addr, unsigned long size, int wc)
{
	dde_kit_addr_t map_addr;
	if (dde_kit_request_mem(phys_addr, size, wc, &map_addr)) {
		PERR("Failed to request I/O memory: [%x,%lx)", phys_addr, phys_addr + size);
		return 0;
	}

	return (void *)map_addr;
}


void *ioremap_wc(resource_size_t phys_addr, unsigned long size)
{
	return _ioremap(phys_addr, size, 1);
}


void *ioremap(resource_size_t offset, unsigned long size)
{
	return _ioremap(offset, size, 0);
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
			dde_kit_log(DEBUG_DRIVER, "MATCH: %s ret: %u match: %p",
			            _drv->name, ret,  _drv->bus->match);
			return ret;
		}

		/**
		 * Probe device with driver
		 */
		int probe(struct device *dev)
		{
			dev->driver = _drv;

			if (dev->bus->probe) {
				dde_kit_log(DEBUG_DRIVER, "Probing device bus");
				return dev->bus->probe(dev);
			} else if (_drv->probe) {
				dde_kit_log(DEBUG_DRIVER, "Probing driver: %s", _drv->name);
				return _drv->probe(dev);
			}

			return 0;
		}
};


int driver_register(struct device_driver *drv)
{
	dde_kit_log(DEBUG_DRIVER, "%s at %p", drv->name, drv);
	new (Genode::env()->heap()) Driver(drv);
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
			dde_kit_log(DEBUG_DRIVER, "Probe return %d", ret);
			if (!ret)
				return 0;
		}

	return 0;
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


struct device *get_device(struct device *dev) { TRACE; return dev; }
const char *dev_name(const struct device *dev) { return dev->name; }


long find_next_zero_bit_le(const void *addr,
                           unsigned long size, unsigned long offset)
{
	unsigned long max_size = sizeof(long) * 8;
	if (offset >= max_size) {
		PWRN("Offset greater max size");
		return offset + size;
	}

	for (; offset < max_size; offset++)
		if (!(*(unsigned long*)addr & (1 << offset)))
			return offset;

	PERR("No zero bit findable");
	return offset + size;
}


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

void put_unaligned_le32(u32 val, void *p)
{
	struct __una_u32 *ptr = (struct __una_u32 *)p;
	ptr->x = val;
}


u64 get_unaligned_le64(const void *p)
{
	struct __una_u64 *ptr = (struct __una_u64 *)p;
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
			return i;

	return 0;
}


/*******************
 ** linux/delay.h **
 *******************/

static Timer::Connection _timer;


void udelay(unsigned long usecs)
{
	_timer.msleep(usecs < 1000 ? 1 : usecs / 1000);
}


void msleep(unsigned int msecs) { _timer.msleep(msecs); }


/*********************
 ** linux/jiffies.h **
 *********************/

/*
 * We use DDE kit's jiffies in 100Hz granularity.
 */
enum { JIFFIES_TICK_MS = 1000 / DDE_KIT_HZ };

unsigned long msecs_to_jiffies(const unsigned int m) { return m / JIFFIES_TICK_MS; }
long time_after_eq(long a, long b) { return (a - b) >= 0; }
long time_after(long a, long b)    { return (b - a) < 0; }

struct dma_pool
{
	size_t size;
	int    align;
};


struct dma_pool *dma_pool_create(const char *name, struct device *d, size_t size,
                                 size_t align, size_t alloc)
{
	dde_kit_log(DEBUG_DMA, "size: %zx align:%zx %p", size, align, __builtin_return_address((0)));

	if (align & (align - 1))
		return 0;

	dma_pool *pool = new(Genode::env()->heap()) dma_pool;
	pool->align    = Genode::log2((int)align);
	pool->size     = size;
	return pool;
}


void  dma_pool_destroy(struct dma_pool *d)
{
	dde_kit_log(DEBUG_DMA, "close");
	destroy(Genode::env()->heap(), d);
}


void *dma_pool_alloc(struct dma_pool *d, gfp_t f, dma_addr_t *dma)
{
	return Malloc::dma()->dma_pool_alloc(d->size, d->align, (Genode::addr_t*)dma);
}


void  dma_pool_free(struct dma_pool *d, void *vaddr, dma_addr_t a)
{
	dde_kit_log(DEBUG_DMA, "free: addr %p, size: %zx", vaddr, d->size);
	Malloc::dma()->dma_pool_free(d->size, vaddr);
}


void *dma_alloc_coherent(struct device *, size_t size, dma_addr_t *dma, gfp_t)
{
	void *addr = Malloc::dma()->alloc(size, PAGE_SHIFT);

	if (!addr)
		return 0;

	*dma = (dma_addr_t)Malloc::dma()->phys_addr(addr);
	dde_kit_log(DEBUG_DMA, "DMA pool alloc addr: %p size %zx align: %d, phys: %lx",
	            addr, size, PAGE_SHIFT, *dma);
	return addr;
}


void dma_free_coherent(struct device *, size_t size, void *vaddr, dma_addr_t)
{
	dde_kit_log(DEBUG_DMA, "free: addr %p, size: %zx", vaddr, size);
	Malloc::dma()->free(vaddr);
}


/*************************
 ** linux/dma-mapping.h **
 *************************/

/**
 * Translate virt to phys using DDE-kit
 */
dma_addr_t dma_map_single_attrs(struct device *dev, void *ptr,
                                size_t size,
                                enum dma_data_direction dir,
                                struct dma_attrs *attrs)
{
	dma_addr_t phys = (dma_addr_t)Malloc::dma()->phys_addr(ptr);

	dde_kit_log(DEBUG_DMA, "virt: %p phys: %lx", ptr, phys);
	return phys;
}


dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        size_t offset, size_t size,
                        enum dma_data_direction dir)
{
	dde_kit_log(DEBUG_DMA, "virt: %p phys: %lx offs: %zx", page->virt, page->phys, offset);
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
	dde_kit_log(DEBUG_THREAD, "Run %s", n);
	Routine::add(fn, arg, n);
	return 0;
}


struct task_struct *kthread_create(int (*threadfn)(void *data),
                                   void *data,
                                   const char namefmt[], ...)
{
	/*
	 * This is just called for delayed device scanning (see
	 * 'drivers/usb/storage/usb.c')
	 */
	dde_kit_log(DEBUG_THREAD, "Create %s", namefmt);
	Routine::add(threadfn, data, namefmt);
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


/****************
 ** Networking **
 ****************/


/*************************
 ** linux/etherdevice.h **
 *************************/

struct net_device *alloc_etherdev(int sizeof_priv)
{
	net_device *dev = new (Genode::env()->heap()) net_device();

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
	if (!(addr[0] & 0x1))
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


