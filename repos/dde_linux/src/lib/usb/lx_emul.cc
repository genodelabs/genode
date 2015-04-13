/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2012-01-29
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
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
#include "routine.h"
#include "signal.h"
#include "platform/lx_mem.h"

#include <extern_c_begin.h>
#include "lx_emul.h"
#include <extern_c_end.h>

/* DDE kit includes */
extern "C" {
#include <dde_kit/semaphore.h>
#include <dde_kit/memory.h>
#include <dde_kit/pgtab.h>
#include <dde_kit/resources.h>
#include <dde_kit/interrupt.h>
#include <dde_kit/thread.h>
}


namespace Genode {
	class Slab_backend_alloc;
	class Slab_alloc;
}

/**
 * Back-end allocator for Genode's slab allocator
 */
class Genode::Slab_backend_alloc : public Genode::Allocator,
                                   public Genode::Rm_connection
{
	private:

		enum {
			VM_SIZE    = 24 * 1024 * 1024,     /* size of VM region to reserve */
			BLOCK_SIZE = 1024  * 1024,         /* 1 MB */
			ELEMENTS   = VM_SIZE / BLOCK_SIZE, /* MAX number of dataspaces in VM */
		};

		addr_t                   _base;              /* virt. base address */
		Genode::Cache_attribute  _cached;            /* non-/cached RAM */
		Ram_dataspace_capability _ds_cap[ELEMENTS];  /* dataspaces to put in VM */
		addr_t                   _ds_phys[ELEMENTS]; /* physical bases of dataspaces */
		int                      _index;             /* current index in ds_cap */
		Allocator_avl            _range;             /* manage allocations */

		bool _alloc_block()
		{
			if (_index == ELEMENTS) {
				PERR("Slab-backend exhausted!");
				return false;
			}

			try {
				_ds_cap[_index] = Backend_memory::alloc(BLOCK_SIZE, _cached);
				/* attach at index * BLOCK_SIZE */
				Rm_connection::attach_at(_ds_cap[_index], _index * BLOCK_SIZE, BLOCK_SIZE, 0);

				/* lookup phys. address */
				_ds_phys[_index] = Dataspace_client(_ds_cap[_index]).phys_addr();
			} catch (...) { return false; }

			/* return base + offset in VM area */
			addr_t block_base = _base + (_index * BLOCK_SIZE);
			++_index;

			_range.add_range(block_base, BLOCK_SIZE);
			return true;
		}

	public:

		Slab_backend_alloc(Genode::Cache_attribute cached)
		: Rm_connection(0, VM_SIZE), _cached(cached), _index(0),
		  _range(env()->heap())
		{
			/* reserver attach us, anywere */
			_base = env()->rm_session()->attach(dataspace());
		}

		/**
		 * Allocate 
		 */
		bool alloc(size_t size, void **out_addr) override
		{
				bool done = _range.alloc(size, out_addr);

				if (done)
					return done;

				done = _alloc_block();
				if (!done) {
					PERR("Backend allocator exhausted\n");
					return false;
				}

				return _range.alloc(size, out_addr);
		}

		void   free(void *addr, size_t /* size */) override { }
		size_t overhead(size_t size) const override { return  0; }
		bool need_size_for_free() const override { return false; }

		/**
		 * Return phys address for given virtual addr.
		 */
		addr_t phys_addr(addr_t addr)
		{
			if (addr < _base || addr >= (_base + VM_SIZE))
				return ~0UL;

			int index = (addr - _base) / BLOCK_SIZE;

			/* physical base of dataspace */
			addr_t phys = _ds_phys[index];

			if (!phys)
				return ~0UL;

			/* add offset */
			phys += (addr - _base - (index * BLOCK_SIZE));
			return phys;
		}

		/**
		 * Translate given physical address to virtual address
		 *
		 * \return virtual address, or 0 if no translation exists
		 */
		addr_t virt_addr(addr_t phys)
		{
			for (unsigned i = 0; i < ELEMENTS; i++) {
				if (_ds_cap[i].valid()
				 && phys >= _ds_phys[i] && phys < _ds_phys[i] + BLOCK_SIZE)
					return _base + i*BLOCK_SIZE + phys - _ds_phys[i];
			}

			PWRN("virt_addr(0x%lx) - no translation", phys);
			return 0;
		}

		addr_t start() const { return _base; }
		addr_t end()   const { return _base + VM_SIZE - 1; }
};


/**
 * Slab allocator using our back-end allocator
 */
class Genode::Slab_alloc : public Genode::Slab
{
	private:

		size_t _calculate_block_size(size_t object_size)
		{
			size_t block_size = 8 * (object_size + sizeof(Slab_entry)) + sizeof(Slab_block);
			return align_addr(block_size, 12);
		}

	public:

		Slab_alloc(size_t object_size, Slab_backend_alloc *allocator)
		: Slab(object_size, _calculate_block_size(object_size), 0, allocator)
    { }

	inline addr_t alloc()
	{
		addr_t result;
		return (Slab::alloc(slab_size(), (void **)&result) ? result : 0);
	}
};


/**
 * Memory interface used used for Linux emulation
 */
class Malloc
{
	private:

		enum {
			SLAB_START_LOG2 = 3,  /* 8 B */
			SLAB_STOP_LOG2  = 16, /* 64 KB */
			NUM_SLABS = (SLAB_STOP_LOG2 - SLAB_START_LOG2) + 1,
		};

		typedef Genode::addr_t addr_t;
		typedef Genode::Slab_alloc Slab_alloc;
		typedef Genode::Slab_backend_alloc Slab_backend_alloc;

		Slab_backend_alloc     *_back_allocator;
		Slab_alloc             *_allocator[NUM_SLABS];
		Genode::Cache_attribute _cached; /* cached or un-cached memory */
		addr_t                  _start;  /* VM region of this allocator */
		addr_t                  _end;

		/**
		 * Set 'value' at 'addr'
		 */
		void _set_at(addr_t addr, addr_t value) { *((addr_t *)addr) = value; }

		/**
		 * Retrieve slab index belonging to given address
		 */
		unsigned _slab_index(Genode::addr_t **addr)
		{
			using namespace Genode;
			/* get index */
			addr_t index = *(*addr - 1);

			/*
			 * If index large, we use aligned memory, retrieve beginning of slab entry
			 * and read index from there
			 */
			if (index > 32) {
				*addr = (addr_t *)*(*addr - 1);
				index = *(*addr - 1);
			}

			return index;
		}

	public:

		Malloc(Slab_backend_alloc *alloc, Genode::Cache_attribute cached)
		: _back_allocator(alloc), _cached(cached), _start(alloc->start()),
		  _end(alloc->end())
		{
			/* init slab allocators */
			for (unsigned i = SLAB_START_LOG2; i <= SLAB_STOP_LOG2; i++)
				_allocator[i - SLAB_START_LOG2] = new (Genode::env()->heap())
				                                  Slab_alloc(1U << i, alloc);
		}

		/**
		 * Alloc in slabs
		 */
		void *alloc(Genode::size_t size, int align = 0, Genode::addr_t *phys = 0)
		{
			using namespace Genode;
			/* += slab index + aligment size */
			size += sizeof(addr_t) + (align > 2 ? (1 << align) : 0);

			int msb = Genode::log2(size);

			if (size > (1U << msb))
				msb++;

			if (size < (1U << SLAB_START_LOG2))
				msb = SLAB_STOP_LOG2;

			if (msb > SLAB_STOP_LOG2) {
				PERR("Slab too large %u reqested %zu cached %d", 1U << msb, size, _cached);
				return 0;
			}

			addr_t addr =  _allocator[msb - SLAB_START_LOG2]->alloc();
			if (!addr) {
				PERR("Failed to get slab for %u", 1 << msb);
				return 0;
			}

			_set_at(addr, msb - SLAB_START_LOG2);
			addr += sizeof(addr_t);

			if (align > 2) {
				/* save */
				addr_t ptr = addr;
				addr_t align_val = (1U << align);
				addr_t align_mask = align_val - 1;
				/* align */
				addr = (addr + align_val) & ~align_mask;
				/* write start address before aligned address */
				_set_at(addr - sizeof(addr_t), ptr);
			}

			if (phys)
				*phys = _back_allocator->phys_addr(addr);
			return (addr_t *)addr;
		}

		void free(void const *a)
		{
			using namespace Genode;
			addr_t *addr = (addr_t *)a;

			unsigned nr = _slab_index(&addr);
			_allocator[nr]->free((void *)(addr - 1));
		}

		Genode::addr_t phys_addr(void *a)
		{
			return _back_allocator->phys_addr((addr_t)a);
		}

		Genode::addr_t virt_addr(Genode::addr_t phys)
		{
			return _back_allocator->virt_addr(phys);
		}

		/**
		 * Belongs given address to this allocator
		 */
		bool inside(addr_t const addr) const { return (addr > _start) && (addr <= _end); }

		/**
		 * Cached memory allocator
		 */
		static Malloc *mem()
		{
			static Slab_backend_alloc _b(Genode::CACHED);
			static Malloc _m(&_b, Genode::CACHED);
			return &_m;
		}

		/**
		 * DMA allocator
		 */
		static Malloc *dma()
		{
			static Slab_backend_alloc _b(Genode::UNCACHED);
			static Malloc _m(&_b, Genode::UNCACHED);
			return &_m;
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

	unsigned long a = (unsigned long)addr;

	if (a & 0x3)
		PERR("Unaligned kmalloc %lx", a);

	//PDBG("Kmalloc: [%lx-%lx) from %p", a, a + size, __builtin_return_address(0));
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
	if (Malloc::mem()->inside((Genode::addr_t)p))
		Malloc::mem()->free(p);
	
	if (Malloc::dma()->inside((Genode::addr_t)p))
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

int    strcmp(const char *s1, const char *s2) { return Genode::strcmp(s1, s2); }
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


void  *kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *ptr = kmalloc(len, gfp);
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
	dde_kit_log(DEBUG_SLAB, "\"%s\" obj_size=%zd", name, size);

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

void *phys_to_virt(unsigned long address)
{
	return (void *)Malloc::dma()->virt_addr(address);
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
			dde_kit_log(DEBUG_DRIVER, "MATCH: %s ret: %u match: %p %p",
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
				dde_kit_log(DEBUG_DRIVER, "Probing device bus %p", dev->bus->probe);
				return dev->bus->probe(dev);
			} else if (_drv->probe) {
				dde_kit_log(DEBUG_DRIVER, "Probing driver: %s %p", _drv->name, _drv->probe);
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


void device_del(struct device *dev)
{
	dde_kit_log(DEBUG_DRIVER, "Remove device %p", dev);
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


long find_next_zero_bit_le(const void *addr,
                           unsigned long size, unsigned long offset)
{
	unsigned long max_size = sizeof(long) * 8;
	if (offset >= max_size) {
		PWRN("Offset greater max size");
		return offset + size;
	}

	for (; offset < max_size; offset++)
		if (!(*(unsigned long*)addr & (1L << offset)))
			return offset;

	PERR("No zero bit findable");
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

static Timer::Connection _timer;


void udelay(unsigned long usecs)
{
	_timer.usleep(usecs);
}


void msleep(unsigned int msecs) { _timer.msleep(msecs); }
void mdelay(unsigned long msecs) { msleep(msecs); }


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
	void *addr;
	addr = dma_alloc_coherent(0, d->size, dma, 0);

	dde_kit_log(DEBUG_DMA, "addr: %p size %zx align %x phys: %lx pool %p",
	            addr, d->size, d->align, *dma, d);
	return addr;
}


void  dma_pool_free(struct dma_pool *d, void *vaddr, dma_addr_t a)
{
	dde_kit_log(DEBUG_DMA, "free: addr %p, size: %zx", vaddr, d->size);
	Malloc::dma()->free(vaddr);
}


void *dma_alloc_coherent(struct device *, size_t size, dma_addr_t *dma, gfp_t)
{
	void *addr = Malloc::dma()->alloc(size, PAGE_SHIFT, dma);

	if (!addr)
		return 0;

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

	if (phys == ~0UL)
		PERR("translation virt->phys %p->%lx failed, return ip %p", ptr, phys,
		     __builtin_return_address(0));

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

struct resource * devm_request_mem_region(struct device *dev, resource_size_t start,
                                          resource_size_t n, const char *name)
{
	struct resource *r = (struct resource *)kzalloc(sizeof(struct resource), GFP_KERNEL);
	r->start = start;
	r->end   = start + n - 1;
	r->name  = name;

	return r;
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


/******************
 ** linux/wait.h **
 ******************/

void init_waitqueue_head(wait_queue_head_t *q)
{
	q->q = 0;
}


void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
	if (q->q) {
		PERR("Non-empty wait queue");
		return;
	}

	q->q = wait;
}


void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
	if (q->q != wait) {
		PERR("Remove unkown element from wait queue");
		return;
	}

	q->q = 0;
}


int waitqueue_active(wait_queue_head_t *q)
{
	return q->q ? 1 : 0;
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
