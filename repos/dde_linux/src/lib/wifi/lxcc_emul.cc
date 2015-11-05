/**
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-03-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/snprintf.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <timer_session/connection.h>
#include <rm_session/connection.h>
#include <rom_session/connection.h>
#include <util/string.h>

/* local includes */
#include <scheduler.h>
#include <firmware_list.h>
#include <lx.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
#include <extern_c_end.h>

static bool const verbose = false;
#define PWRNV(...) do { if (verbose) PWRN(__VA_ARGS__); } while (0)

typedef Genode::size_t size_t;
typedef Genode::addr_t addr_t;


namespace Lx {
	class Slab_backend_alloc;
	class Slab_alloc;
}

/**
 * Back-end allocator for Genode's slab allocator
 */
class Lx::Slab_backend_alloc : public Genode::Allocator,
                               public Genode::Rm_connection
{
	private:

		enum {
			VM_SIZE    = 24 * 1024 * 1024,     /* size of VM region to reserve */
			BLOCK_SIZE = 1024  * 1024,         /* 1 MiB */
			ELEMENTS   = VM_SIZE / BLOCK_SIZE, /* MAX number of dataspaces in VM */
		};

		addr_t                           _base;              /* virt. base address */
		Genode::Cache_attribute          _cached;            /* non-/cached RAM */
		Genode::Ram_dataspace_capability _ds_cap[ELEMENTS];  /* dataspaces to put in VM */
		addr_t                           _ds_phys[ELEMENTS]; /* physical bases of dataspaces */
		int                              _index;             /* current index in ds_cap */
		Genode::Allocator_avl            _range;             /* manage allocations */

		bool _alloc_block()
		{
			if (_index == ELEMENTS) {
				PERR("Slab-backend exhausted!");
				return false;
			}

			try {
				_ds_cap[_index] = Lx::backend_alloc(BLOCK_SIZE, _cached);
				/* attach at index * BLOCK_SIZE */
				Rm_connection::attach_at(_ds_cap[_index], _index * BLOCK_SIZE, BLOCK_SIZE, 0);

				/* lookup phys. address */
				_ds_phys[_index] = Genode::Dataspace_client(_ds_cap[_index]).phys_addr();
			} catch (...) { return false; }

			/* return base + offset in VM area */
			addr_t block_base = _base + (_index * BLOCK_SIZE);
			++_index;

			_range.add_range(block_base, BLOCK_SIZE);
			return true;
		}

	public:

		Slab_backend_alloc(Genode::Cache_attribute cached)
		:
			Rm_connection(0, VM_SIZE),
			_cached(cached), _index(0), _range(Genode::env()->heap())
		{
			/* reserver attach us, anywere */
			_base = Genode::env()->rm_session()->attach(dataspace());
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
				if (_ds_cap[i].valid() &&
				    phys >= _ds_phys[i] && phys < _ds_phys[i] + BLOCK_SIZE)
					return _base + i*BLOCK_SIZE + phys - _ds_phys[i];
			}

			PWRN("virt_addr(0x%lx) - no translation", phys);
			return 0;
		}

		addr_t start() const { return _base; }
		addr_t end()   const { return _base + VM_SIZE - 1; }

		/**
		 * Cached memory backend allocator
		 */
		static Slab_backend_alloc & mem()
		{
			static Slab_backend_alloc inst(Genode::CACHED);
			return inst;
		}

		/**
		 * DMA memory backend allocator
		 */
		static Slab_backend_alloc & dma()
		{
			static Slab_backend_alloc inst(Genode::UNCACHED);
			return inst;
		}
};


/**
 * Slab allocator using our back-end allocator
 */
class Lx::Slab_alloc : public Genode::Slab
{
	private:

		/*
		 * Each slab block in the slab contains about 8 objects (slab entries)
		 * as proposed in the paper by Bonwick and block sizes are multiples of
		 * page size.
		 */
		static size_t _calculate_block_size(size_t object_size)
		{
			size_t block_size = 8 * (object_size + sizeof(Genode::Slab_entry))
			                                     + sizeof(Genode::Slab_block);
			return Genode::align_addr(block_size, 12);
		}

	public:

		Slab_alloc(size_t object_size, Slab_backend_alloc &allocator)
		: Slab(object_size, _calculate_block_size(object_size), 0, &allocator) { }

		/**
		 * Convenience slabe-entry allocation
		 */
		addr_t alloc()
		{
			addr_t result;
			return (Slab::alloc(slab_size(), (void **)&result) ? result : 0);
		}
};


/**
 * Memory interface used for Linux emulation
 */
class Malloc
{
	private:

		enum {
			SLAB_START_LOG2 = 3,  /* 8 B */
			SLAB_STOP_LOG2  = 16, /* 64 KiB */
			NUM_SLABS = (SLAB_STOP_LOG2 - SLAB_START_LOG2) + 1,
		};

		typedef Genode::addr_t         addr_t;
		typedef Lx::Slab_alloc         Slab_alloc;
		typedef Lx::Slab_backend_alloc Slab_backend_alloc;

		Slab_backend_alloc     &_back_allocator;
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

		/**
		 * Get the originally requested size of the allocation
		 */
		size_t _get_orig_size(Genode::addr_t **addr)
		{
			using namespace Genode;

			addr_t index = *(*addr - 1);
			if (index > 32) {
				*addr = (addr_t *) * (*addr - 1);
			}

			return *(*addr - 2);
		}

	public:

		Malloc(Slab_backend_alloc &alloc, Genode::Cache_attribute cached)
		:
			_back_allocator(alloc), _cached(cached), _start(alloc.start()),
			_end(alloc.end())
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

			/* save requested size */
			size_t orig_size = size;
			size += sizeof(addr_t);

			/* += slab index + aligment size */
			size += sizeof(addr_t) + (align > 2 ? (1 << align) : 0);

			int msb = Genode::log2(size);

			if (size > (1U << msb))
				msb++;

			if (size < (1U << SLAB_START_LOG2))
				msb = SLAB_STOP_LOG2;

			if (msb > SLAB_STOP_LOG2) {
				// PERR("Slab too large %u reqested %zu cached %d", 1U << msb, size, _cached);
				return 0;
			}

			addr_t addr =  _allocator[msb - SLAB_START_LOG2]->alloc();
			if (!addr) {
				PERR("Failed to get slab for %u", 1 << msb);
				return 0;
			}

			_set_at(addr, orig_size);
			addr += sizeof(addr_t);

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
				*phys = _back_allocator.phys_addr(addr);
			return (addr_t *)addr;
		}

		void free(void const *a)
		{
			using namespace Genode;
			addr_t *addr = (addr_t *)a;

			/* XXX changes addr */
			unsigned nr = _slab_index(&addr);
			/* we need to decrease addr by 2, orig_size and index come first */
			_allocator[nr]->free((void *)(addr - 2));
		}

		size_t size(void const *a)
		{
			using namespace Genode;
			addr_t *addr = (addr_t *)a;

			/* XXX changes addr */
			return _get_orig_size(&addr);
		}

		Genode::addr_t phys_addr(void *a)
		{
			return _back_allocator.phys_addr((addr_t)a);
		}

		Genode::addr_t virt_addr(Genode::addr_t phys)
		{
			return _back_allocator.virt_addr(phys);
		}

		/**
		 * Belongs given address to this allocator
		 */
		bool inside(addr_t const addr) const { return (addr > _start) && (addr <= _end); }

		/**
		 * Cached memory allocator
		 */
		static Malloc & mem()
		{
			static Malloc inst(Slab_backend_alloc::mem(), Genode::CACHED);
			return inst;
		}

		/**
		 * DMA memory allocator
		 */
		static Malloc & dma()
		{
			static Malloc inst(Slab_backend_alloc::dma(), Genode::UNCACHED);
			return inst;
		}
};


void Lx::debug_printf(int level, char const *fmt, ...)
{
	if (level) {
		va_list va;
		va_start(va, fmt);
		Genode::vprintf(fmt, va);
		va_end(va);
	}
}


void Lx::printf(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
}


extern "C" void lx_printf(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
}


extern "C" void lx_vprintf(char const *fmt, va_list va) {
	Genode::vprintf(fmt, va); }


/********************
 ** linux/string.h **
 ********************/

size_t strlen(const char *s)
{
	return Genode::strlen(s);
}


int strcmp(const char* s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}


int strncmp(const char *s1, const char *s2, size_t len)
{
	return Genode::strcmp(s1, s2, len);
}


char *strchr(const char *p, int ch)
{
	char c;
	c = ch;
	for (;; ++p) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}


void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (unsigned char *)s;
	while (n-- != 0) {
		if ((unsigned char)c == *p++) {
			return (void *)(p - 1);
		}
	}
	return NULL;
}


char *strnchr(const char *p, size_t count, int ch)
{
	char c;
	c = ch;
	for (; count; ++p, count--) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}


char *strcpy(char *dst, const char *src)
{
	char *p = dst;

	while ((*dst = *src)) {
		++src;
		++dst;
	}

	return p;
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


int sprintf(char *str, const char *format, ...)
{
	enum { BUFFER_LEN = 128 };
	va_list list;

	va_start(list, format);
	Genode::String_console sc(str, BUFFER_LEN);
	sc.vprintf(format, list);
	va_end(list);

	return sc.len();
}


int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list list;

	va_start(list, format);
	Genode::String_console sc(str, size);
	sc.vprintf(format, list);
	va_end(list);

	return sc.len();
}


int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
	Genode::String_console sc(str, size);
	sc.vprintf(format, args);

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


size_t strnlen(const char *s, size_t maxlen)
{
	size_t c;
	for (c = 0; c <maxlen; c++)
		if (!s[c])
			return c;

	return maxlen;
}


char *kasprintf(gfp_t ftp, const char *fmt, ...)
{
	/* for now, we hope strings are not getting longer than 128 bytes */
	enum { MAX_STRING_LENGTH = 128 };
	char *p = (char*)kmalloc(MAX_STRING_LENGTH, 0);
	if (!p)
		return 0;

	va_list args;

	va_start(args, fmt);
	Genode::String_console sc(p, MAX_STRING_LENGTH);
	sc.vprintf(fmt, args);
	va_end(args);

	return p;
}


void *memcpy(void *dst, const void *src, size_t n)
{
	Genode::memcpy(dst, src, n);

	return dst;
}


void *memmove(void *dst, const void *src, size_t n)
{
	Genode::memmove(dst, src, n);

	return dst;
}


void *memset(void *s, int c, size_t n)
{
	Genode::memset(s, c, n);

	return s;
}


/*****************
 ** linux/uio.h **
 *****************/

int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len)
{
	while (len > 0) {
		if (iov->iov_len) {
			size_t copy_len = (size_t)len < iov->iov_len ? len : iov->iov_len;
			Genode::memcpy(kdata, iov->iov_base, copy_len);

			len -= copy_len;
			kdata += copy_len;
			iov->iov_base = (unsigned char *)iov->iov_base + copy_len;
			iov->iov_len -= copy_len;
		}
		iov++;
	}

	return 0;
}


int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len)
{
	while (len > 0) {
		if (iov->iov_len) {
			size_t copy_len = (size_t)len < iov->iov_len ? len : iov->iov_len;
			Genode::memcpy(iov->iov_base, kdata, copy_len);

			len -= copy_len;
			kdata += copy_len;
			iov->iov_base = (unsigned char *)iov->iov_base + copy_len;
			iov->iov_len -= copy_len;
		}
		iov++;
	}

	return 0;
}


/********************
 ** linux/socket.h **
 ********************/

extern "C" int memcpy_fromiovecend(unsigned char *kdata, const struct iovec *iov,
                                   int offset, int len)
{
	while (offset >= (int)iov->iov_len) {
		offset -= iov->iov_len;
		iov++;
	}

	while (len > 0) {
		u8 *base = ((u8*) iov->iov_base) + offset;
		size_t copy_len = len < (int)iov->iov_len - offset ? len : iov->iov_len - offset;

		offset = 0;
		Genode::memcpy(kdata, base, copy_len);

		len -= copy_len;
		kdata += copy_len;
		iov++;
	}

	return 0;
}


/**********************
 ** Memory allocation *
 **********************/

void *kmalloc(size_t size, gfp_t flags)
{
	if (flags & __GFP_DMA)
		PWRN("GFP_DMA memory (below 16 MiB) requested (%p)", __builtin_return_address(0));
	if (flags & __GFP_DMA32)
		PWRN("GFP_DMA32 memory (below 4 GiB) requested (%p)", __builtin_return_address(0));

	void *addr = flags & GFP_LX_DMA ? Malloc::dma().alloc(size, 12)
	                                : Malloc::mem().alloc(size);

	if ((addr_t)addr & 0x3)
		PERR("unaligned kmalloc %lx", (addr_t)addr);

	if (flags & __GFP_ZERO)
		Genode::memset(addr, 0, size);

	return addr;
}


void *kzalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags | __GFP_ZERO);
}


void *kzalloc_node(size_t size, gfp_t flags, int node)
{
	return kzalloc(size, 0);
}


void *kcalloc(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > (~0UL / size))
		return 0;

	return kzalloc(n * size, flags);
}


void kfree(void const *p)
{
	if (!p) return;

	if (Malloc::mem().inside((Genode::addr_t)p))
		Malloc::mem().free(p);
	else if (Malloc::dma().inside((Genode::addr_t)p))
		Malloc::dma().free(p);
	else
		PERR("%s: unknown block at %p, called from %p", __func__,
		     p, __builtin_return_address(0));
}


void kzfree(void const *p)
{
	if (!p) return;

	size_t len = ksize(const_cast<void*>(p));

	Genode::memset((void*)p, 0, len);

	kfree(p);
}


void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node)
{
	return kmalloc(size, flags);
}


static size_t _ksize(void *p)
{
	size_t size = 0;

	if (Malloc::mem().inside((Genode::addr_t)p))
		size = Malloc::mem().size(p);
	else if (Malloc::dma().inside((Genode::addr_t)p))
		size = Malloc::dma().size(p);
	else
		PERR("%s: unknown block at %p", __func__, p);

	return size;
}


size_t ksize(void *p)
{
	return _ksize(p);
}


void *krealloc(void *p, size_t size, gfp_t flags)
{
	/* XXX handle short-cut where size == old_size */
	void *addr = kmalloc(size, flags);

	if (addr && p) {
		size_t old_size = _ksize(p);

		Genode::memcpy(addr, p, old_size);
		kfree(p);
	}

	return addr;
}


void *kmemdup(const void *src, size_t size, gfp_t flags)
{
	void *addr = kmalloc(size, flags);

	if (addr)
		Genode::memcpy(addr, src, size);

	return addr;
}


/******************
 ** linux/slab.h **
 ******************/

struct kmem_cache : Lx::Slab_alloc
{
	kmem_cache(size_t object_size, bool dma)
	:
		Lx::Slab_alloc(object_size, dma ? Lx::Slab_backend_alloc::dma()
		                                : Lx::Slab_backend_alloc::mem())
	{ }
};


struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long flags, void (*ctor)(void *))
{
	if (ctor) {
		PERR("%s: ctor not supported", __func__);
		return nullptr;
	}

	return new (Genode::env()->heap()) kmem_cache(size, flags & SLAB_LX_DMA);
}


void * kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
	return (void *)cache->alloc();
}


void kmem_cache_free(struct kmem_cache *cache, void *objp)
{
	cache->free(objp);
}


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size)
{
	size_t real_size = size + sizeof(size_t);
	size_t *addr;
	try { addr = (size_t *)Genode::env()->heap()->alloc(real_size); }
	catch (...) { return 0; }

	*addr = real_size;
	return addr + 1;
}


void vfree(const void *addr)
{
	if (!addr) return;

	size_t size = *(((size_t *)addr) - 1);
	Genode::env()->heap()->free(const_cast<void *>(addr), size);
}


/********************
 ** linux/string.h **
 ********************/

int memcmp(const void *p0, const void *p1, size_t size) {
	return Genode::memcmp(p0, p1, size); }


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
			return ret;
		}

		/**
		 * Probe device with driver
		 */
		int probe(struct device *dev)
		{
			dev->driver = _drv;

			if (dev->bus->probe) {
				return dev->bus->probe(dev);
			} else if (_drv->probe) {
				return _drv->probe(dev);
			}

			return 0;
		}
};


int driver_register(struct device_driver *drv)
{
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


const char *dev_name(const struct device *dev) { return dev->name; }


int dev_set_name(struct device *dev, const char *fmt, ...)
{
	enum { MAX_DEV_LEN = 64 };
	/* XXX needs to be freed */
	char *name = (char*)kmalloc(MAX_DEV_LEN, 0);
	if (!name)
		return 1;

	va_list list;
	va_start(list, fmt);
	Genode::String_console sc(name, MAX_DEV_LEN);
	sc.vprintf(fmt, list);

	va_end(list);

	dev->name = name;
	return 0;
}


/********************
 ** linux/kernel.h **
 ********************/

int strict_strtoul(const char *s, unsigned int base, unsigned long *res)
{
	unsigned long r = -EINVAL;
	Genode::ascii_to_unsigned(s, r, base);
	*res = r;

	return r;
}


/*******************
 ** linux/delay.h **
 *******************/

static Timer::Connection _timer;


void udelay(unsigned long usecs)
{
	_timer.usleep(usecs);

	Lx::scheduler().current()->schedule();
}


void usleep_range(unsigned long min, unsigned long max)
{
	_timer.usleep(min);

	Lx::scheduler().current()->schedule();
}


void msleep(unsigned int msecs)
{
	_timer.msleep(msecs);

	Lx::scheduler().current()->schedule();
}


void mdelay(unsigned long msecs) { msleep(msecs); }


/*********************
 ** linux/jiffies.h **
 *********************/

enum {
	JIFFIES_TICK_MS = 1000/HZ,
	JIFFIES_TICK_US = 1000*1000/HZ,
};

unsigned long msecs_to_jiffies(const unsigned int m) { return m / JIFFIES_TICK_MS; }
unsigned int jiffies_to_msecs(const unsigned long j) { return j * JIFFIES_TICK_MS; }
unsigned long usecs_to_jiffies(const unsigned int u) { return u / JIFFIES_TICK_US; }


/*******************
 ** linux/timer.h **
 *******************/

static unsigned long round_jiffies(unsigned long j, bool force_up)
{
	unsigned remainder = j % HZ;

	/*
	 * from timer.c
	 *
	 * If the target jiffie is just after a whole second (which can happen
	 * due to delays of the timer irq, long irq off times etc etc) then
	 * we should round down to the whole second, not up. Use 1/4th second
	 * as cutoff for this rounding as an extreme upper bound for this.
	 * But never round down if @force_up is set.
	 */

	/* per default round down */
	j = j - remainder;

	/* round up if remainder more than 1/4 second (or if we're forced to) */
	if (remainder >= HZ/4 || force_up)
		j += HZ;

	return j;
}

unsigned long round_jiffies(unsigned long j)
{
	return round_jiffies(j, false);
}


unsigned long round_jiffies_up(unsigned long j)
{
	return round_jiffies(j, true);
}


unsigned long round_jiffies_relative(unsigned long j)
{
	return round_jiffies(j + jiffies, false) - jiffies;
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


/**********************
 ** linux/firmware.h **
 **********************/

extern Firmware_list fw_list[];
extern size_t        fw_list_len;


int request_firmware_nowait(struct module *module, bool uevent,
                            const char *name, struct device *device,
                            gfp_t gfp, void *context,
                            void (*cont)(const struct firmware *, void *))
{
	/* only try to load known firmware images */
	Firmware_list *fwl = 0;
	for (size_t i = 0; i < fw_list_len; i++) {
		if (Genode::strcmp(name, fw_list[i].name) == 0) {
			fwl = &fw_list[i];
			break;
		}
	}

	if (!fwl) {
		PERR("firmware '%s' is not in the firmware white list.", name);
		return -1;
	}

	Genode::Rom_connection rom(fwl->name);
	Genode::Dataspace_capability ds_cap = rom.dataspace();

	if (!ds_cap.valid())
		return -1;

	firmware *fw = (firmware *)kzalloc(sizeof (firmware), 0);
	if (!fw) {
		PERR("could not allocate memory for struct firmware");
		return -1;
	}

	/* use Genode env because our slab only goes up to 64KiB */
	fw->data = (u8*)Genode::env()->heap()->alloc(fwl->size);
	if (!fw->data) {
		PERR("could not allocate memory for firmware image");
		kfree(fw);
		return -1;
	}

	void const *image = Genode::env()->rm_session()->attach(ds_cap);
	Genode::memcpy((void*)fw->data, image, fwl->size);
	Genode::env()->rm_session()->detach(image);

	fw->size = fwl->size;

	cont(fw, context);

	return 0;
}

void release_firmware(const struct firmware *fw)
{
	Genode::env()->heap()->free(const_cast<u8 *>(fw->data), fw->size);
	kfree(fw);
}


/*************************
 ** linux/dma-mapping.h **
 *************************/

void *dma_alloc_coherent(struct device *dev, size_t size,
                         dma_addr_t *dma_handle, gfp_t flag)
{
	dma_addr_t dma_addr;
	void *addr = Malloc::dma().alloc(size, 12, &dma_addr);

	if (!addr) {
		// PERR("dma alloc: %zu failed", size);
		return 0;
	}

	*dma_handle = dma_addr;

	return addr;
}


void *dma_zalloc_coherent(struct device *dev, size_t size,
                          dma_addr_t *dma_handle, gfp_t flag)
{
	void *addr = dma_alloc_coherent(dev, size, dma_handle, flag);

	if (addr)
		Genode::memset(addr, 0, size);

	return addr;
}


void dma_free_coherent(struct device *dev, size_t size,
                       void *vaddr, dma_addr_t dma_handle)
{
	if (Malloc::dma().inside((Genode::addr_t)vaddr))
		Malloc::dma().free(vaddr);
	else
		PERR("vaddr: %p is not DMA memory", vaddr);
}


dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        size_t offset, size_t size,
                        enum dma_data_direction direction)
{
	if (!Malloc::dma().inside((Genode::addr_t)page->addr))
		PERR("page->page: %p not DMA address", page->addr);

	dma_addr_t dma_addr = (dma_addr_t) Malloc::dma().phys_addr(page->addr);

	if (dma_addr == ~0UL)
		PERR("%s: virtual address %p not registered for DMA, called from: %p",
		     __func__, page->addr, __builtin_return_address(0));

	return dma_addr;
}

dma_addr_t dma_map_single(struct device *dev, void *cpu_addr, size_t size,
                          enum dma_data_direction direction)
{
	dma_addr_t dma_addr = (dma_addr_t) Malloc::dma().phys_addr(cpu_addr);

	if (dma_addr == ~0UL)
		PERR("%s: virtual address %p not registered for DMA, called from: %p",
		     __func__, cpu_addr, __builtin_return_address(0));


	return dma_addr;
}


int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return (dma_addr == ~0UL) ? 1 : 0;
}


/********************
 ** linux/dcache.h **
 ********************/

unsigned int full_name_hash(const unsigned char *name, unsigned int len)
{
	unsigned hash = 0, i;
	for (i = 0; i < len; i++)
		hash += name[i];

	return hash;
}


/******************
 ** linux/hash.h **
 ******************/

u32 hash_32(u32 val, unsigned int bits)
{
	enum  { GOLDEN_RATIO_PRIME_32 = 0x9e370001UL };
	u32 hash = val * GOLDEN_RATIO_PRIME_32;

	hash =  hash >> (32 - bits);
	return hash;
}


/*****************
 ** linux/gfp.h **
 *****************/

class Addr_to_page_mapping : public Genode::List<Addr_to_page_mapping>::Element
{
	private:

		unsigned long _addr { 0 };
		struct page   *_page { 0 };

		static Genode::List<Addr_to_page_mapping> *_list()
		{
			static Genode::List<Addr_to_page_mapping> _l;
			return &_l;
		}

	public:

		Addr_to_page_mapping(unsigned long addr, struct page *page)
		: _addr(addr), _page(page) { }

		static void insert(struct page *page)
		{
			Addr_to_page_mapping *m = (Addr_to_page_mapping*)
				Malloc::mem().alloc(sizeof (Addr_to_page_mapping));

			m->_addr = (unsigned long)page->addr;
			m->_page = page;

			_list()->insert(m);
		}

		static void remove(struct page *page)
		{
			Addr_to_page_mapping *mp = 0;
			for (Addr_to_page_mapping *m = _list()->first(); m; m = m->next())
				if (m->_page == page)
					mp = m;

			if (mp) {
				_list()->remove(mp);
				Malloc::mem().free(mp);
			}
		}

		static struct page* find_page(unsigned long addr)
		{
			for (Addr_to_page_mapping *m = _list()->first(); m; m = m->next())
				if (m->_addr == addr)
					return m->_page;

			return 0;
		}
};


unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	struct page *p = alloc_pages(gfp_mask, 0);
	if (!p)
		return 0UL;

	Genode::memset(p->addr, 0, PAGE_SIZE);

	return (unsigned long)p->addr;
}


struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page = (struct page *)kzalloc(sizeof(struct page), 0);

	size_t size = PAGE_SIZE << order;

	page->addr = Malloc::dma().alloc(size, 12);

	if (!page->addr) {
		PERR("alloc_pages: %zu failed", size);
		kfree(page);
		return 0;
	}

	Addr_to_page_mapping::insert(page);

	atomic_set(&page->_count, 1);

	return page;
}


void __free_pages(struct page *page, unsigned int order)
{
	if (!atomic_dec_and_test(&page->_count)) {
		PWRNV("attempting to free page  %p with _count: %d, called from: %p",
		     page, atomic_read(&page->_count), __builtin_return_address(0));
		return;
	}

	Addr_to_page_mapping::remove(page);

	Malloc::dma().free(page->addr);
	kfree(page);
}


void free_pages(unsigned long page, unsigned int order)
{
	struct page *p = Addr_to_page_mapping::find_page(page);
	__free_pages(p, order);
}


/****************
 ** linux/mm.h **
 ****************/

struct page *virt_to_head_page(const void *addr)
{
	struct page *page = Addr_to_page_mapping::find_page((unsigned long)addr);
	if (!page) {
		/**
		 * Linux uses alloc_pages() to allocate memory but passes addr + offset
		 * to the caller (e.g. __netdev_alloc_frag()). Therefore, we also try to
		 * find the aligned addr in our page mapping list.
		 */
		unsigned long aligned_addr = (unsigned long)addr & ~0xfff;
		page = Addr_to_page_mapping::find_page(aligned_addr);
		if (!page) {
			PERR("BUG: addr: %p and aligned addr: %p have no page mapping, "
			     " called from: %p", addr, (void*)aligned_addr,
			     __builtin_return_address(0));
			Genode::sleep_forever();
		}
	}

	return page;
}


void get_page(struct page *page)
{
	atomic_inc(&page->_count);
}


void put_page(struct page *page)
{
	if (!atomic_dec_and_test(&page->_count))
		return;

	Malloc::dma().free(page->addr);
	kfree(page);
}


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
			return offset;

	return size;
}


unsigned long find_next_zero_bit(unsigned long const *addr, unsigned long size,
                                 unsigned long offset)
{
	unsigned long i, j;

	for (i = offset; i < (size / BITS_PER_LONG); i++)
		if (addr[i] != ~0UL)
			break;

	if (i == size)
		return size;

	for (j = 0; j < BITS_PER_LONG; j++)
		if ((~addr[i]) & (1UL << j))
			break;

	return (i * BITS_PER_LONG) + j;
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


/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align)
{
	return kmalloc(size, 0);
}


/*******************************
 **  net/core/net/namespace.h **
 *******************************/

int register_pernet_subsys(struct pernet_operations *ops)
{
	if (ops->init)
		ops->init(&init_net);

	return 0;
}


int register_pernet_device(struct pernet_operations *ops)
{
	return register_pernet_subsys(ops);
}


/**************************
 ** core/net_namespace.c **
 **************************/

DEFINE_MUTEX(net_mutex);


/*******************
 ** kernel/kmod.c **
 *******************/

extern "C" void module_iwl_init(void);
extern "C" void module_iwl_mvm_init(void);


int __request_module(bool wait, char const *format, ...)
{
	va_list list;
	char buf[128];

	va_start(list, format);
	Genode::String_console sc(buf, sizeof(buf));
	sc.vprintf(format, list);
	va_end(list);

	return 0;
}


/* XXX request_module() should not hardcode module names */
int request_module(char const* format, ...)
{
	va_list list;
	char buf[128];

	va_start(list, format);
	Genode::String_console sc(buf, sizeof(buf));
	sc.vprintf(format, list);
	va_end(list);

	if (Genode::strcmp(buf, "iwldvm", 6) == 0) {
		module_iwl_init();
		return 0;
	}
	else if (Genode::strcmp(buf, "iwlmvm", 6) == 0) {
		module_iwl_mvm_init();
		return 0;
	}
	else if (Genode::strcmp(buf, "ccm(aes)", 7) == 0) {
		return 0;
	}
	else if (Genode::strcmp(buf, "cryptomgr", 9) == 0) {
		return 0;
	}

	return -1;
}


/****************************
 ** kernel/locking/mutex.c **
 ****************************/

enum { MUTEX_UNLOCKED = 1, MUTEX_LOCKED = 0, MUTEX_WAITERS = -1 };

void mutex_init(struct mutex *m)
{
	static unsigned id = 0;

	m->state   = MUTEX_UNLOCKED;
	m->holder  = nullptr;
	m->waiters = new (Genode::env()->heap()) Lx::Task::List;
	m->id      = ++id;
}


void mutex_destroy(struct mutex *m)
{
	/* FIXME potentially blocked tasks are not unblocked */

	Genode::destroy(Genode::env()->heap(), static_cast<Lx::Task::List *>(m->waiters));

	m->holder  = nullptr;
	m->waiters = nullptr;
	m->id      = 0;
}


void mutex_lock(struct mutex *m)
{
	while (1) {
		if (m->state == MUTEX_UNLOCKED) {
			m->state  = MUTEX_LOCKED;
			m->holder = Lx::scheduler().current();

			break;
		}

		Lx::Task *t = reinterpret_cast<Lx::Task *>(m->holder);

		if (t == Lx::scheduler().current()) {
			PERR("Bug: mutex does not support recursive locking");
			Genode::sleep_forever();
		}

		/* notice that a task waits for the mutex to be released */
		m->state = MUTEX_WAITERS;

		/* block until the mutex is released (and retry then) */
		Lx::scheduler().current()->mutex_block(static_cast<Lx::Task::List *>(m->waiters));
		Lx::scheduler().current()->schedule();
	}
}


void mutex_unlock(struct mutex *m)
{
	if (m->state == MUTEX_UNLOCKED) {
		PERR("Bug: multiple mutex unlock detected");
		Genode::sleep_forever();
	}
	if (m->holder != Lx::scheduler().current()) {
		PERR("Bug: mutex unlock by task not holding the mutex");
		Genode::sleep_forever();
	}

	Lx::Task::List *waiters = static_cast<Lx::Task::List *>(m->waiters);

	if (m->state == MUTEX_WAITERS)
		while (Lx::Task::List_element *le = waiters->first())
			le->object()->mutex_unblock(waiters);

	m->state  = MUTEX_UNLOCKED;
	m->holder = nullptr;
}


int mutex_is_locked(struct mutex *m)
{
	return m->state != MUTEX_UNLOCKED;
}


int mutex_trylock(struct mutex *m)
{
	if (mutex_is_locked(m))
		return false;

	mutex_lock(m);
	return true;
}


/******************
 ** linux/poll.h **
 ******************/

bool poll_does_not_wait(const poll_table *p)
{
	return p == nullptr;
}


/*********************
 ** linux/kthread.h **
 *********************/

void *kthread_run(int (*threadfn)(void *), void *data, char const *name)
{
	threadfn(data);

	return (void*)42;
}
