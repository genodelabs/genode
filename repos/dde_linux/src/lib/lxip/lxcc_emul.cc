/**
 * \brief  Linux emulation code
 * \author Sebastian Sumpf
 * \date   2013-08-28
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <base/allocator_avl.h>
#include <base/snprintf.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <timer_session/connection.h>
#include <trace/timestamp.h>

#include <lx/extern_c_begin.h>
#include <lx_emul.h>
#include <lx/extern_c_end.h>

#include <env.h>


/*
 * VM-area to reserve for back-end allocator
 */
enum { VM_RESERVATION = 24 * 1024 * 1024 };

/*
 * TODO: fine tune these
 */
unsigned long totalram_pages = VM_RESERVATION / PAGE_SIZE;
unsigned long num_physpages  = totalram_pages;

namespace Genode {
	template <unsigned VM_SIZE> class Slab_backend_alloc;
	typedef Slab_backend_alloc<VM_RESERVATION> Backend_alloc;
	class Slab_alloc;
}

/**
 * Back-end allocator for Genode's slab allocator
 */
template <unsigned VM_SIZE>
class Genode::Slab_backend_alloc : public Genode::Allocator,
                                   public Genode::Rm_connection
{
	private:

		enum {
			BLOCK_SIZE = 1024  * 1024,         /* 1 MB */
			ELEMENTS   = VM_SIZE / BLOCK_SIZE, /* MAX number of dataspaces in VM */
		};

		addr_t                   _base;              /* virt. base address */
		Cache_attribute          _cached;            /* non-/cached RAM */
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
				_ds_cap[_index] =  Genode::env()->ram_session()->alloc(BLOCK_SIZE, _cached);
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

		Slab_backend_alloc(Cache_attribute cached)
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

		Slab_alloc(size_t object_size, Backend_alloc *allocator)
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
		typedef Genode::Slab_backend_alloc<VM_RESERVATION> Slab_backend_alloc;

		Slab_backend_alloc *_back_allocator;
		Slab_alloc         *_allocator[NUM_SLABS]; 
		bool                _cached; /* cached or un-cached memory */
		addr_t              _start;  /* VM region of this allocator */
		addr_t              _end;

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
				PERR("Slab too large %u requested %zu cached %d", 1U << msb, size, _cached);
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

		size_t slab_size(void const *a)
		{
			using namespace Genode;
			addr_t *addr =(addr_t *)a;

			unsigned nr = _slab_index(&addr);
			size_t size = 1 << (SLAB_START_LOG2 + nr);
			size -= (addr_t)a - (addr_t)(addr - 1);
			return size;
		}

		Genode::addr_t phys_addr(void *a)
		{
			return _back_allocator->phys_addr((addr_t)a);
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
};


/*************************************
 ** Memory allocation, linux/slab.h **
 *************************************/

void *kmalloc(size_t size, gfp_t flags)
{
	void *addr = Malloc::mem()->alloc(size);

	unsigned long a = (unsigned long)addr;

	if (a & 0x3)
		PERR("Unaligned kmalloc %lx", a);

//	PDBG("Kmalloc: [%lx-%lx) from %p", a, a + size, __builtin_return_address(0));
	return addr;
}


void *kzalloc(size_t size, gfp_t flags)
{
	void *addr = kmalloc(size, flags);
	if (addr)
		Genode::memset(addr, 0, size);

	//PDBG("Kmalloc: [%lx-%lx) from %p", (unsigned)addr, (unsigned)addr + size, __builtin_return_address(0));
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
	if (!p) return;

	if (Malloc::mem()->inside((Genode::addr_t)p))
		Malloc::mem()->free(p);
	else 
		PWRN("%p is not within our memory range called from %p",
		     p, __builtin_return_address(0));
}


void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node)
{
	return kmalloc(size, 0);
}


void *kzalloc_node(size_t size, gfp_t flags, int node)
{
	return kzalloc(size, 0);
}


size_t ksize(const void *p)
{
	if (!(Malloc::mem()->inside((Genode::addr_t)p))) {
		PDBG("%p not in slab allocator", p);
		return 0;
	}

	size_t size =  Malloc::mem()->slab_size(p);
	return size;
}


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

	void *table = Genode::env()->heap()->alloc(elements * bucketsize);

	if (_hash_mask)
		*_hash_mask = (1 << nlog2) - 1;
	if (_hash_shift)
		*_hash_shift = nlog2;

	return table;
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
int    strcmp(const char *s1, const char *s2) { return Genode::strcmp(s1, s2); }
int    strncmp(const char *s1, const char *s2, size_t len) { 
	return Genode::strcmp(s1, s2, len); }

int    memcmp(const void *p0, const void *p1, size_t size) {
	return Genode::memcmp(p0, p1, size); }



int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Genode::String_console sc(str, size);

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


void *kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *ptr = kmalloc(len, gfp);
	Genode::memcpy(ptr, src, len);
	return ptr;
}


void *genode_memcpy(void *d, const void *s, size_t n)
{
	return Genode::memcpy(d, s, n);
}


/******************
 ** linux/log2.h **
 ******************/

unsigned long ilog2(unsigned long n) { return Genode::log2<unsigned long>(n); }


/*******************
 ** linux/sched.h **
 *******************/


static void __wait_event(signed long timeout)
{
		static Timer::Connection timer;
		/* timeout is relative in jiffies, make it absolute */
		timeout += jiffies;

		 /* wait for signal and return upon timeout */
		while (timeout > jiffies  && !Net::Env::receiver()->pending())
		{
			timer.msleep(1);
			update_jiffies();

			if (timeout <= jiffies)
				return;
		}

		/* dispatch signal */
		Genode::Signal s = Net::Env::receiver()->wait_for_signal();
		static_cast<Genode::Signal_dispatcher_base *>(s.context())->dispatch(s.num());
}


long schedule_timeout_uninterruptible(signed long timeout)
{
	return schedule_timeout(timeout);
}


signed long schedule_timeout(signed long timeout)
{
	long start = jiffies;
	__wait_event(timeout);
	timeout -= jiffies - start;
	return timeout < 0 ? 0 : timeout;
}

void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
	__wait_event(0);
}


/******************
 ** linux/time.h **
 ******************/

unsigned long get_seconds(void)
{
	return jiffies / HZ;
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

			lx_log(DEBUG_SLAB, "alloc page: %p addr: %lx-%lx", _page, _addr, _addr + size);
		}

		virtual ~Avl_page()
		{
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

static Genode::Avl_tree<Avl_page> tree;

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	Avl_page *p;
	try {
		p = (Avl_page *)new (Genode::env()->heap()) Avl_page(PAGE_SIZE << order);
		tree.insert(p);
	} catch (...) { return 0; }

	return p->page();
}


/****************
 ** linux/mm.h **
 ****************/

struct page *virt_to_head_page(const void *x)
{
	Avl_page *p = tree.first()->find_by_address((Genode::addr_t)x);
	lx_log(DEBUG_SLAB, "virt_to_head_page: %p page %p\n", x,p ? p->page() : 0);
	
	return p ? p->page() : 0;
}


void put_page(struct page *page)
{
	if (!atomic_dec_and_test(&page->_count))
		return;

	lx_log(DEBUG_SLAB, "put_page: %p", page);
	Avl_page *p = tree.first()->find_by_address((Genode::addr_t)page->addr);

	tree.remove(p);
	destroy(Genode::env()->heap(), p);
}


static void create_event(char const *fmt, va_list list)
{
	enum { BUFFER_LEN = 64, EVENT_LEN = BUFFER_LEN + 32 };
	char buf[BUFFER_LEN];

	using namespace Genode;

	String_console sc(buf, BUFFER_LEN);
	sc.vprintf(fmt, list);

	char event[EVENT_LEN];
	static Trace::Timestamp last = 0;
	       Trace::Timestamp now  = Trace::timestamp();
	Genode::snprintf(event, sizeof(event), "delta = %llu ms %s",
	                 (now - last) / 2260000, buf);
	Thread_base::trace(event);
	last = now;
}


extern "C" void lx_trace_event(char const *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	create_event(fmt, list);
	va_end(list);
}
