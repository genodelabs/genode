/*
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-03-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/snprintf.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <timer_session/connection.h>
#include <region_map/client.h>
#include <rom_session/connection.h>
#include <util/bit_allocator.h>
#include <util/string.h>

/* local includes */
#include <lx.h>
#include <lx_emul.h>

#include <lx_kit/env.h>
#include <lx_kit/malloc.h>
#include <lx_kit/scheduler.h>


typedef ::size_t       size_t;
typedef Genode::addr_t addr_t;



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


size_t copy_from_iter(void *addr, size_t bytes, struct iov_iter *i)
{
	if (bytes > i->count)
		bytes = i->count;

	if (bytes == 0)
		return 0;

	char             *kdata = reinterpret_cast<char*>(addr);
	struct iovec const *iov = i->iov;

	size_t len = bytes;
	while (len > 0) {
		if (iov->iov_len) {
			size_t copy_len = (size_t)len < iov->iov_len ? len : iov->iov_len;
			Genode::memcpy(kdata, iov->iov_base, copy_len);

			len -= copy_len;
			kdata += copy_len;
		}
		iov++;
	}

	return bytes;
}


bool copy_from_iter_full(void *addr, size_t bytes, struct iov_iter *i)
{
	/* XXX at some point check if i->count > bytes could be a problem */
	if (bytes > i->count)
		bytes = i->count;

	if (bytes == 0)
		return true;

	size_t const copied = copy_from_iter(addr, bytes, i);
	if (copied != bytes) {
		Genode::error(__func__, ":", __LINE__, " could not copy all bytes");
		return false;
	}

	return true;
}


size_t copy_to_iter(void *addr, size_t bytes, struct iov_iter *i)
{
	if (bytes > i->count)
		bytes = i->count;

	if (bytes == 0)
		return 0;

	char             *kdata = reinterpret_cast<char*>(addr);
	struct iovec const *iov = i->iov;

	size_t len = bytes;
	while (len > 0) {
		if (iov->iov_len) {
			size_t copy_len = (size_t)len < iov->iov_len ? len : iov->iov_len;
			Genode::memcpy(iov->iov_base, kdata, copy_len);

			len -= copy_len;
			kdata += copy_len;
		}
		iov++;
	}

	return bytes;
}


size_t copy_page_to_iter(struct page *page, size_t offset, size_t bytes,
                         struct iov_iter *i)
{
	return copy_to_iter(reinterpret_cast<unsigned char*>(page->addr) + offset, bytes, i);
}


size_t copy_page_from_iter(struct page *page, size_t offset, size_t bytes,
                           struct iov_iter *i)
{
	return copy_from_iter(reinterpret_cast<unsigned char*>(page->addr) + offset, bytes, i);
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

#include <lx_emul/impl/slab.h>

void *kvmalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags);
}


void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > SIZE_MAX / size) return NULL;
	return kmalloc(n * size, flags);
}


void kvfree(const void *p)
{
	kfree(p);
}


void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return kzalloc(size, gfp | GFP_LX_DMA);
}


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size)
{
	size_t real_size = size + sizeof(size_t);
	size_t *addr;
	
	if (!Lx_kit::env().heap().alloc(real_size, (void**)&addr)) {
		return nullptr;
	}

	*addr = real_size;
	return addr + 1;
}


void *vzalloc(unsigned long size)
{
	void *addr = vmalloc(size);

	if (addr)
		memset(addr, 0, size);

	return addr;
}


void vfree(const void *addr)
{
	if (!addr) return;

	size_t size = *(((size_t *)addr) - 1);
	Lx_kit::env().heap().free(const_cast<void *>(addr), size);
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
	new (&Lx_kit::env().heap()) Driver(drv);
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
	dev->driver_data = data;
	return 0;
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

#include <lx_emul/impl/delay.h>


void usleep_range(unsigned long min, unsigned long max)
{
	udelay(min);
}


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


/*******************
 ** linux/ktime.h **
 *******************/

ktime_t ktime_get_real(void)
{
	return (ktime_t) (s64)(jiffies * (1000 / HZ) * NSEC_PER_MSEC);
}


ktime_t ktime_sub(ktime_t const lhs, ktime_t const rhs)
{
	return lhs - rhs;
}


struct timespec ktime_to_timespec(ktime_t const nsec)
{
	struct timespec ts;
	if (!nsec) { return (struct timespec) {0, 0}; }

	/* XXX check nsec < NSEC_PER_SEC */
	ts.tv_sec = nsec / NSEC_PER_SEC;
	ts.tv_nsec = (nsec % NSEC_PER_SEC) * (1000*1000);

	return ts;
}


bool ktime_to_timespec_cond(ktime_t const kt, struct timespec *ts)
{
	if (kt) {
		*ts = ktime_to_timespec(kt);
		return true;
	}

	return false;
}


struct timeval ns_to_timeval(ktime_t const nsec)
{
	struct timespec ts = ktime_to_timespec(nsec);
	struct timeval tv;

	tv.tv_sec = ts.tv_sec;
	tv.tv_usec = ts.tv_nsec / 1000;

	return tv;
}


/*************************
 ** linux/timekeeping.h **
 *************************/

time64_t ktime_get_seconds(void)
{
	return jiffies_to_msecs(jiffies) / 1000;
}


/*************************
 ** linux/dma-mapping.h **
 *************************/

/* use a smaller limit then possible to cover potential overhead */
enum { DMA_LARGE_ALLOC_SIZE = 60u << 10, };

void *dma_alloc_coherent(struct device *dev, size_t size,
                         dma_addr_t *dma_handle, gfp_t flag)
{
	bool const large_alloc = size >= DMA_LARGE_ALLOC_SIZE;
	dma_addr_t dma_addr = 0;
	void *addr = large_alloc ? Lx::Malloc::dma().alloc_large(size)
	                         : Lx::Malloc::dma().alloc(size, 12, &dma_addr);

	if (addr) {
		*dma_handle = large_alloc ? Lx::Malloc::dma().phys_addr(addr)
		                          : dma_addr;
	}
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
	if (size >= DMA_LARGE_ALLOC_SIZE) {
		Lx::Malloc::dma().free_large(vaddr);
		return;
	}

	if (Lx::Malloc::dma().inside((Genode::addr_t)vaddr)) {
		Lx::Malloc::dma().free(vaddr);
	} else {
		Genode::error("vaddr: ", vaddr, " is not DMA memory");
	}
}


dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        size_t offset, size_t size,
                        enum dma_data_direction direction)
{
	if (!Lx::Malloc::dma().inside((Genode::addr_t)page->addr)) {
		Genode::error(__func__, ": virtual address ", (void*)page->addr, " not an DMA address");
	}

	dma_addr_t dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(page->addr);

	if (dma_addr == ~0UL) {
		Genode::error(__func__, ": virtual address ", (void*)page->addr,
		              " not registered for DMA");
	}

	return dma_addr;
}

dma_addr_t dma_map_single(struct device *dev, void *cpu_addr, size_t size,
                          enum dma_data_direction direction)
{
	dma_addr_t dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(cpu_addr);

	if (dma_addr == ~0UL) {
		Genode::error(__func__, ": virtual address ", cpu_addr,
		              " not registered for DMA ", __builtin_return_address(0));
		BUG();
	}

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
				Lx::Malloc::mem().alloc(sizeof (Addr_to_page_mapping));

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
				Lx::Malloc::mem().free(mp);
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

	page->addr = Lx::Malloc::dma().alloc(size, 12);

	if (!page->addr) {
		Genode::error("alloc_pages: ", size, " failed");
		kfree(page);
		return 0;
	}

	Addr_to_page_mapping::insert(page);

	atomic_set(&page->_count, 1);

	return page;
}


void *__alloc_page_frag(struct page_frag_cache *nc,
                        unsigned int fragsz, gfp_t gfp_mask)
{
	struct page *page = alloc_pages(gfp_mask, fragsz / PAGE_SIZE);
	if (!page) return nullptr;

	return page->addr;
}


void *page_frag_alloc(struct page_frag_cache *nc, unsigned int fragsz, gfp_t gfp_mask)
{
	return __alloc_page_frag(nc, fragsz, gfp_mask);
}


void page_frag_free(void *addr)
{
	__free_page_frag(addr);
}


void __free_page_frag(void *addr)
{
	struct page *page = virt_to_head_page(addr);
	__free_pages(page, 0xdeadbeef);
}


void __free_pages(struct page *page, unsigned int order)
{
	if (!atomic_dec_and_test(&page->_count))
		/* reference counter did not drop to zero - do not free yet */
		return;

	Addr_to_page_mapping::remove(page);

	Lx::Malloc::dma().free(page->addr);
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
			Genode::error("BUG: addr: ", addr, " and aligned addr: ",
			              (void*)aligned_addr, " have no page mapping, ");
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
	if (!page) {
		Genode::warning(__func__, ": page is zero");
		return;
	}

	if (!atomic_dec_and_test(&page->_count))
		return;

	Lx::Malloc::dma().free(page->addr);
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


/**********************
 ** net/ns/generic.h **
 **********************/

void *net_generic(const struct net *net, unsigned int id)
{
	if (id >= MAX_NET_GENERIC_PTR) {
		Genode::error(__func__, ":", " id ", id, " invalid");
		return NULL;
	}

	struct net_generic *ng = net->gen;
	void *ptr = ng->ptr[id];
	if (!ptr) {
		Genode::error(__func__, ":", " cannot get ptr");
		BUG();
	}

	return ptr;
}


/*******************************
 **  net/core/net/namespace.h **
 *******************************/

int register_pernet_subsys(struct pernet_operations *ops)
{
	if (!init_net.gen) {
		init_net.gen = (struct net_generic*)kzalloc(
			offsetof(struct net_generic, ptr[MAX_NET_GENERIC_PTR]), 0);
		if (!init_net.gen) {
			Genode::error("could not allocate net_generic memory");
			return -1;
		}
	}

	if (ops->id && ops->size) {
		/* XXX AFAICS there is only netlink_tap_net_ops that requires it */
		unsigned int id = *ops->id;
		if (id >= MAX_NET_GENERIC_PTR) {
			Genode::error(__func__, ":", " id ", id, " invalid");
			return -1;
		}

		void *data = kzalloc(ops->size, 0);
		init_net.gen->ptr[id] = data;
	}

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

#include <lx_emul/impl/mutex.h>


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


/*****************
 ** linux/pci.h **
 *****************/

#include <lx_emul/impl/pci.h>


void *pci_get_drvdata(struct pci_dev *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}


void pci_set_drvdata(struct pci_dev *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}


static struct pcim_iomap_devres {
	void *table[6];
} _devres_table;


int pcim_iomap_regions_request_all(struct pci_dev *pdev, int mask, const char *name)
{
	/* XXX iwlwifi just want to map the first BAR */
	void *addr = pci_ioremap_bar(pdev, 0);
	if (!addr) { return -1; }

	printk("%s:%d from: %p addr: %p\n", __func__, __LINE__, __builtin_return_address(0), addr);

	_devres_table.table[0] = addr;
	return 0;
}


void * const *pcim_iomap_table(struct pci_dev *pdev)
{
	return _devres_table.table;
}


/***********************
 ** linux/interrupt.h **
 ***********************/

#include <lx_kit/irq.h>


int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *dev)
{
	Lx::Pci_dev *pci_dev = Lx::pci_dev_registry()->first();
	Lx::Irq::irq().request_irq(pci_dev->client(), irq, handler, dev);
	return 0;
}


int request_threaded_irq(unsigned int irq, irq_handler_t handler,
                         irq_handler_t thread_fn,
                         unsigned long flags, const char *name,
                         void *dev)
{
	Lx::Pci_dev *pci_dev = Lx::pci_dev_registry()->first();
	Lx::Irq::irq().request_irq(pci_dev->client(), irq, handler, dev, thread_fn);
	return 0;
}


void pci_dev_put(struct pci_dev *pci_dev)
{
	Genode::destroy(Lx_kit::env().heap(), pci_dev);
}


/***********************
 ** linux/workqueue.h **
 ***********************/

#include <lx_emul/impl/work.h>


bool mod_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork,
                      unsigned long delay)
{
	queue_delayed_work(wq, dwork, delay);
	return true;
}


struct workqueue_struct *alloc_ordered_workqueue(char const *fmt , unsigned int flags, ...)
{
	return alloc_workqueue(fmt, flags, 1);
}


struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	Lx::Work *work = Lx::Work::alloc_work_queue(&Lx::Malloc::mem(), fmt);
	wq->task       = (void *)work;

	return wq;
}


void flush_workqueue(struct workqueue_struct *wq)
{
	Lx::Task *current = Lx::scheduler().current();
	if (!current) {
		Genode::error("BUG: flush_workqueue executed without task");
		Genode::sleep_forever();
	}

	Lx::Work *lx_work = (wq && wq->task) ? (Lx::Work*) wq->task
	                                     : &Lx::Work::work_queue();

	lx_work->flush(*current);
	Lx::scheduler().current()->block_and_schedule();
}


static inline bool work_queued(struct workqueue_struct *wq, void *work)
{
	Lx::Work *lx_work = (wq && wq->task) ? (Lx::Work*) wq->task
	                                     : &Lx::Work::work_queue();
	return lx_work->work_queued(work);
}


bool flush_work(struct work_struct *work)
{
	/* XXX AFAIU if the work was not queued it is already 'idle' and
	 *     we just return false
	 */
	bool const queued = work_queued(work->wq, work);
	if (queued) {

		struct workqueue_struct *wq = work->wq;

		Lx::Work *lx_work = (wq && wq->task) ? (Lx::Work*) wq->task
		                                     : &Lx::Work::work_queue();

		Lx::Task *current = Lx::scheduler().current();
		lx_work->wakeup_for(work, *current);

		Lx::scheduler().current()->block_and_schedule();
		return true;
	}

	return false;
}


bool flush_delayed_work(struct delayed_work *dwork)
{
	/* XXX AFAIU if the work was not queued it is already 'idle' and
	 *     we just return false
	 */
	bool const queued = work_queued(dwork->wq, dwork);
	if (queued) {
		Genode::error(__func__, " dwork: ", dwork, " (", dwork->work.func, ") queued");
		Genode::sleep_forever();
		return true;
	}

	return false;
}


/***********************
 ** linux/interrupt.h **
 ***********************/

void tasklet_init(struct tasklet_struct *t, void (*f)(unsigned long), unsigned long d)
{
	t->func = f;
	t->data = d;
}


void tasklet_schedule(struct tasklet_struct *tasklet)
{
	Lx::Work::work_queue().schedule_tasklet(tasklet);
	Lx::Work::work_queue().unblock();
}


/************************
 ** linux/completion.h **
 ************************/

#include <lx_emul/impl/completion.h>


long __wait_completion(struct completion *work, unsigned long timeout) {
	return timeout ? 1 : 0; }


int wait_for_completion_killable(struct completion *work)
{
	__wait_completion(work, 0);
	return 0;
}


long wait_for_completion_killable_timeout(struct completion *work,
                                          unsigned long timeout)
{
	__wait_completion(work, 0);
	return 1;
}


/******************
 ** linux/wait.h **
 ******************/

#include <lx_emul/impl/wait.h>


/*******************
 ** linux/timer.h **
 *******************/

#include <lx_emul/impl/timer.h>


void init_timer_deferrable(struct timer_list *timer) { /* XXX */ }


signed long schedule_timeout_uninterruptible(signed long timeout) { return 0; }


int wake_up_process(struct task_struct *tsk) { return 0; }


/*******************
 ** linux/sched.h **
 *******************/

#include <lx_emul/impl/sched.h>


/*****************
 ** linux/idr.h **
 *****************/

struct Idr
{
	enum { INVALID_ENTRY = ~0ul, };
	enum { MAX_ENTRIES = 1024, };

	Genode::Bit_array<MAX_ENTRIES>  _barray { };
	addr_t                          _ptr[MAX_ENTRIES] { };
	void                           *_idp { nullptr };

	bool _check(addr_t index) { return index < MAX_ENTRIES ? true : false; }

	Idr(struct idr *idp) : _idp(idp) { }

	virtual ~Idr() { }

	bool handles(void *ptr) { return _idp == ptr; }

	bool set_id(addr_t index, void *ptr)
	{
		if (_barray.get(index, 1)) { return false; }

		_barray.set(index, 1);
		_ptr[index] = ptr;
		return true;
	}

	addr_t alloc(addr_t start, void *ptr)
	{
		addr_t index = INVALID_ENTRY;
		for (addr_t i = start; i < MAX_ENTRIES; i++) {
			if (_barray.get(i, 1)) { continue; }
			index = i;
			break;
		}

		if (index == INVALID_ENTRY) { return INVALID_ENTRY; }

		_barray.set(index, 1);
		_ptr[index] = (addr_t) ptr;
		return index;
	}

	void clear(addr_t index)
	{
		if (!_check(index)) { return; }

		_barray.clear(index, 1);
		_ptr[index] = 0;
	}

	addr_t next(addr_t index)
	{
		for (addr_t i = index; i < MAX_ENTRIES; i++) {
			if (_barray.get(i, 1)) { return i; }
		}
		return INVALID_ENTRY;
	}

	void *get_ptr(addr_t index)
	{
		if (!_check(index)) { return NULL; }
		return (void*)_ptr[index];
	}
};


static Genode::Registry<Genode::Registered<Idr>> _idr_registry;


static Idr &idp_to_idr(struct idr *idp)
{
	Idr *idr = nullptr;
	auto lookup = [&](Idr &i) {
		if (i.handles(idp)) { idr = &i; }
	};
	_idr_registry.for_each(lookup);

	if (!idr) {
		Genode::Registered<Idr> *i = new (&Lx_kit::env().heap())
			Genode::Registered<Idr>(_idr_registry, idp);
		idr = &*i;
	}

	return *idr;
}


int idr_alloc(struct idr *idp, void *ptr, int start, int end, gfp_t gfp_mask)
{
	Idr &idr = idp_to_idr(idp);

	if ((end - start) > 1) {
		addr_t const id = idr.alloc(start, ptr);
		return id != Idr::INVALID_ENTRY ? id : -1;
	} else {
		if (idr.set_id(start, ptr)) { return start; }
	}

	return -1;
}


void *idr_find(struct idr *idp, int id)
{
	Idr &idr = idp_to_idr(idp);
	return idr.get_ptr(id);
}


void *idr_get_next(struct idr *idp, int *nextid)
{
	Idr &idr = idp_to_idr(idp);
	addr_t i = idr.next(*nextid);
	if (i == Idr::INVALID_ENTRY) { return NULL; }

	*nextid = i;
	return idr.get_ptr(i);
}


/****************************
 ** asm-generic/getorder.h **
 ****************************/

int get_order(unsigned long size)
{
	if (size < PAGE_SIZE) { return 0; }
	return Genode::log2(size) - PAGE_SHIFT;
}
