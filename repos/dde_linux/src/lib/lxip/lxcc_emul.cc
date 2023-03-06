/*
 * \brief  Linux emulation code
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2013-08-28
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/object_pool.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <trace/timestamp.h>
#include <timer_session/connection.h>

/* format-string includes */
#include <format/snprintf.h>

/* local includes */
#include <lx_emul.h>
#include <lx.h>

/* Lx_kit */
#include <legacy/lx_kit/env.h>

/*********************************
 ** Lx::Backend_alloc interface **
 *********************************/

#include <legacy/lx_kit/backend_alloc.h>

struct Memory_object_base;

static Lx_kit::Env *lx_env;

static Genode::Object_pool<Memory_object_base> *memory_pool_ptr;


void Lx::lxcc_emul_init(Lx_kit::Env &env)
{
	static Genode::Object_pool<Memory_object_base> memory_pool;

	memory_pool_ptr = &memory_pool;

	lx_env = &env;

	LX_MUTEX_INIT(dst_gc_mutex);
	LX_MUTEX_INIT(proto_list_mutex);
}


struct Memory_object_base : Genode::Object_pool<Memory_object_base>::Entry
{
	Memory_object_base(Genode::Ram_dataspace_capability cap)
	: Genode::Object_pool<Memory_object_base>::Entry(cap) {}

	void free() { lx_env->ram().free(ram_cap()); }

	Genode::Ram_dataspace_capability ram_cap()
	{
		using namespace Genode;
		return reinterpret_cap_cast<Ram_dataspace>(cap());
	}
};


Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache)
{
	using namespace Genode;

	Genode::Ram_dataspace_capability cap = lx_env->ram().alloc(size);
	Memory_object_base *o = new (lx_env->heap()) Memory_object_base(cap);

	memory_pool_ptr->insert(o);
	return cap;
}


void Lx::backend_free(Genode::Ram_dataspace_capability cap)
{
	using namespace Genode;

	Memory_object_base *object;
	memory_pool_ptr->apply(cap, [&] (Memory_object_base *o) {
		if (!o) return;

		o->free();
		memory_pool_ptr->remove(o);

		object = o; /* save for destroy */
	});
	destroy(lx_env->heap(), object);
}


Genode::addr_t Lx::backend_dma_addr(Genode::Ram_dataspace_capability)
{
	return 0;
}


/*************************************
 ** Memory allocation, linux/slab.h **
 *************************************/

#include <legacy/lx_emul/impl/slab.h>


void *alloc_large_system_hash(const char *tablename,
	                            unsigned long bucketsize,
	                            unsigned long numentries,
	                            int scale,
	                            int flags,
	                            unsigned int *_hash_shift,
	                            unsigned int *_hash_mask,
	                            unsigned long low_limit,
	                            unsigned long high_limit)
{
	unsigned long elements = numentries ? numentries : high_limit;
	unsigned long nlog2 = ilog2(elements);
	nlog2 <<= (1 << nlog2) < elements ? 1 : 0;


	return lx_env->heap().try_alloc(elements * bucketsize).convert<void *>(

		[&] (void *table_ptr) {

			if (_hash_mask)
				*_hash_mask = (1 << nlog2) - 1;

			if (_hash_shift)
				*_hash_shift = nlog2;

			return table_ptr;
		},

		[&] (Genode::Allocator::Alloc_error) -> void * {
			Genode::error("alloc_large_system_hash allocation failed");
			return nullptr;
		}
	);
}


void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > SIZE_MAX / size) return NULL;
	return kmalloc(n * size, flags);
}


/********************
 ** linux/slab.h   **
 ********************/

void *kmem_cache_alloc_node(struct kmem_cache *cache, gfp_t flags, int node)
{
	return (void*)cache->alloc_element();
}

void *kmem_cache_zalloc(struct kmem_cache *cache, gfp_t flags)
{
	void *addr = (void*)cache->alloc_element();
	if (addr) { memset(addr, 0, cache->size()); }

	return addr;
}


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size)
{
	return kmalloc(size, 0);
}


void vfree(void const *addr)
{
	kfree(addr);
}


/********************
 ** linux/string.h **
 ********************/

char *strcpy(char *to, const char *from)
{
	char *save = to;
	for (; (*to = *from); ++from, ++to);
 	return(save);
}


char *strncpy(char *dst, const char* src, size_t n)
{
	Genode::copy_cstring(dst, src, n);
	return dst;
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


size_t strnlen(const char *s, size_t maxlen)
{
	size_t c;
	for (c = 0; c < maxlen; c++)
		if (!s[c])
			return c;

	return maxlen;
}


size_t strlen(const char *s) { return Genode::strlen(s); }


int strcmp(const char *s1, const char *s2) { return Genode::strcmp(s1, s2); }


int strncmp(const char *s1, const char *s2, size_t len) {
	return Genode::strcmp(s1, s2, len); }


int memcmp(const void *p0, const void *p1, size_t size) {
	return Genode::memcmp(p0, p1, size); }


int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Format::String_console sc(str, size);
	sc.vprintf(format, list);
	va_end(list);

	return sc.len();
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


/* from linux/lib/string.c */
char *strstr(char const *s1, char const *s2)
{
	size_t l1, l2;

	l2 = strlen(s2);
	if (!l2)
		return (char *)s1;
	l1 = strlen(s1);
	while (l1 >= l2) {
		l1--;
		if (!memcmp(s1, s2, l2))
			return (char *)s1;
		s1++;
	}
	return NULL;
}

void *memset(void *s, int c, size_t n)
{
	return Genode::memset(s, c, n);
}


void *memcpy(void *d, const void *s, size_t n)
{
	return Genode::memcpy(d, s, n);
}


void *memmove(void *d, const void *s, size_t n)
{
	return Genode::memmove(d, s, n);
}


/*****************
 ** linux/gfp.h **
 *****************/

class Avl_page : public Genode::Avl_node<Avl_page>
{
	private:

		Genode::addr_t  _addr;
		Genode::size_t  _size;
		struct page    *_page;

	public:

		Avl_page(Genode::size_t size) : _size(size)
		{
			_addr =(Genode::addr_t)kmalloc(size, 0);
			if (!_addr)
				throw -1;

			_page = (struct page *) kzalloc(sizeof(struct page), 0);
			if (!_page) {
				kfree((void *)_addr);
				throw -2;
			}

			_page->addr = (void *)_addr;
			atomic_set(&_page->_count, 1);

			lx_log(DEBUG_SLAB, "alloc page: %p addr: %lx-%lx", _page, _addr, _addr + _size);
		}

		virtual ~Avl_page()
		{
			lx_log(DEBUG_SLAB, "free page: %p addr: %lx-%lx", _page, _addr, _addr + _size);
			kfree((void *)_addr);
			kfree((void *)_page);
		}

		struct page* page() { return _page; }

		bool higher(Avl_page *c)
		{
			return c->_addr > _addr;
		}

		Avl_page *find_by_address(Genode::addr_t addr)
		{
			if (addr >= _addr && addr < _addr + _size)
				return this;

			bool side = addr > _addr;
			Avl_page *c = Avl_node<Avl_page>::child(side);
			return c ? c->find_by_address(addr) : 0;
		}
};


static Genode::Avl_tree<Avl_page> & tree()
{
	static Genode::Avl_tree<Avl_page> _tree;
	return _tree;
}


struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	Avl_page *p;
	try {
		p = (Avl_page *)new (lx_env->heap()) Avl_page(PAGE_SIZE << order);
		tree().insert(p);
	} catch (...) { return 0; }

	return p->page();
}


void *__alloc_page_frag(struct page_frag_cache *nc,
                        unsigned int fragsz, gfp_t gfp_mask)
{
	struct page *page = alloc_pages(gfp_mask, fragsz / PAGE_SIZE);
	if (!page) return nullptr;

	return page->addr;
}


void __free_page_frag(void *addr)
{
	Avl_page *p = tree().first()->find_by_address((Genode::addr_t)addr);

	tree().remove(p);
	destroy(lx_env->heap(), p);
}


/****************
 ** linux/mm.h **
 ****************/

struct page *virt_to_head_page(const void *x)
{
	Avl_page *p = tree().first()->find_by_address((Genode::addr_t)x);
	lx_log(DEBUG_SLAB, "virt_to_head_page: %p page %p\n", x,p ? p->page() : 0);
	
	return p ? p->page() : 0;
}


void put_page(struct page *page)
{
	if (!atomic_dec_and_test(&page->_count))
		return;

	lx_log(DEBUG_SLAB, "put_page: %p", page);
	Avl_page *p = tree().first()->find_by_address((Genode::addr_t)page->addr);

	tree().remove(p);
	destroy(lx_env->heap(), p);
}


static void create_event(char const *fmt, va_list list)
{
	enum { BUFFER_LEN = 64, EVENT_LEN = BUFFER_LEN + 32 };
	char buf[BUFFER_LEN];

	using namespace Genode;

	Format::String_console sc(buf, BUFFER_LEN);
	sc.vprintf(fmt, list);

	char event[EVENT_LEN];
	static Trace::Timestamp last = 0;
	       Trace::Timestamp now  = Trace::timestamp();
	Format::snprintf(event, sizeof(event), "delta = %llu ms %s",
	                 (now - last) / 2260000, buf);
	Thread::trace(event);
	last = now;
}


extern "C" void lx_trace_event(char const *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	create_event(fmt, list);
	va_end(list);
}


/*****************
 ** linux/uio.h **
 *****************/

static inline size_t _copy_iter(void *addr, size_t bytes,
                                struct iov_iter *i, bool to_iter)
{
	if (addr == nullptr) { return 0; }

	if (i->count == 0 ||
	    i->iov == nullptr ||
	    i->iov->iov_len == 0) {
		return 0;
	}

	if (i->nr_segs > 1) {
		Genode::error(__func__, ": too many segments ", i->nr_segs);
		return 0;
	}

	/* make sure the whole iter fits as there is only 1 iovec */
	if (i->iov->iov_len < i->count) {
		Genode::error(__func__, ": "
		              "iov->iov_len: ", i->iov->iov_len, " < "
		              "i->count: ", i->count);
		return 0;
	}

	struct iovec const * const iov = i->iov;
	size_t const           iov_len = iov->iov_len;
	void * const              base = (iov->iov_base + i->iov_offset);

	if (bytes > i->count) { bytes = i->count; }

	size_t const         len = (size_t)(bytes < iov_len ? bytes : iov_len);
	void * const         dst = to_iter ? base : addr;
	void const * const   src = to_iter ? addr : base;

	/* actual function body */
	{
		Genode::memcpy(dst, src, len);
	}

	i->iov_offset += len;
	i->count      -= len;

	return len;
}


size_t copy_from_iter(void *addr, size_t bytes, struct iov_iter *i)
{
	return _copy_iter(addr, bytes, i, false);
}


size_t copy_to_iter(void *addr, size_t bytes, struct iov_iter *i)
{
	return _copy_iter(addr, bytes, i, true);
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


static size_t _csum_and_copy_iter(void *addr, size_t bytes, __wsum *csum,
                                  struct iov_iter *i, bool to_iter)
{
	if (addr == nullptr) { return 0; }

	if (i->count == 0 ||
	    i->iov == nullptr ||
	    i->iov->iov_len == 0) {
		return 0;
	}

	if (i->nr_segs > 1) {
		Genode::error(__func__, ": too many segments ", i->nr_segs);
		return 0;
	}

	/* make sure the whole iter fits as there is only 1 iovec */
	if (i->iov->iov_len < i->count) {
		Genode::error(__func__, ": "
		              "iov->iov_len: ", i->iov->iov_len, " < "
		              "i->count: ", i->count);
		return 0;
	}

	struct iovec const * const iov = i->iov;
	size_t const           iov_len = iov->iov_len;
	void * const              base = (iov->iov_base + i->iov_offset);

	if (bytes > i->count) { bytes = i->count; }

	size_t const         len = (size_t)(bytes < iov_len ? bytes : iov_len);
	void * const         dst = to_iter ? base : addr;
	void const * const   src = to_iter ? addr : base;

	/* actual function body */
	{
		int err = 0;
		__wsum next = csum_and_copy_from_user(src, dst, len, 0, &err);

		if (err) {
			Genode::error(__func__, ": err: ", err, " - sleeping");
			Genode::sleep_forever();
		}

		*csum = csum_block_add(*csum, next, 0);
	}

	i->iov_offset += len;
	i->count      -= len;

	return len;
}


size_t csum_and_copy_from_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i)
{
	return _csum_and_copy_iter(addr, bytes, csum, i, false);
}


size_t csum_and_copy_to_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i)
{
	return _csum_and_copy_iter(addr, bytes, csum, i, true);
}


/******************
 ** linux/wait.h **
 ******************/

void __wake_up(wait_queue_head_t *q, bool all) { }


/***********************
 ** linux/workqueue.h **
 ***********************/

static void execute_delayed_work(unsigned long dwork)
{
	delayed_work *d = (delayed_work *)dwork;
	d->work.func(&d->work);
}


bool mod_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork,
                      unsigned long delay)
{
	/* treat delayed work without delay like any other work */
	if (delay == 0) {
		execute_delayed_work((unsigned long)dwork);
	} else {
		if (!dwork->timer.function) {
			setup_timer(&dwork->timer, execute_delayed_work,
			            (unsigned long)dwork);
		}
		mod_timer(&dwork->timer, jiffies + delay);
	}
	return true;
}

int schedule_delayed_work(struct delayed_work *dwork, unsigned long delay)
{
	return mod_delayed_work(0, dwork, delay);
}
