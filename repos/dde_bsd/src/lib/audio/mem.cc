/**
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-16
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/log.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <util/string.h>

/* local includes */
#include <bsd.h>
#include <bsd_emul.h>


static bool const verbose = false;


namespace Bsd {
	typedef Genode::addr_t addr_t;

	class Slab_backend_alloc;
	class Slab_alloc;
	class Malloc;
}


/**
 * Back-end allocator for Genode's slab allocator
 */
class Bsd::Slab_backend_alloc : public Genode::Allocator,
                                public Genode::Rm_connection,
                                public Genode::Region_map_client
{
	private:

		enum {
			VM_SIZE    = 8 * 1024 * 1024,      /* size of VM region to reserve */
			BLOCK_SIZE =     1024 * 1024,      /* 1 MiB */
			ELEMENTS   = VM_SIZE / BLOCK_SIZE, /* MAX number of dataspaces in VM */
		};

		addr_t                            _base;              /* virt. base address */
		Genode::Ram_dataspace_capability  _ds_cap[ELEMENTS];  /* dataspaces to put in VM */
		addr_t                            _ds_phys[ELEMENTS]; /* physical bases of dataspaces */
		int                               _index;             /* current index in ds_cap */
		Genode::Allocator_avl             _range;             /* manage allocations */
		Genode::Ram_allocator            &_ram;               /* allocator to allocate ds from */

		bool _alloc_block()
		{
			if (_index == ELEMENTS) {
				Genode::error("Slab-backend exhausted!");
				return false;
			}

			try {
				_ds_cap[_index] = _ram.alloc(BLOCK_SIZE);
				Region_map_client::attach_at(_ds_cap[_index], _index * BLOCK_SIZE, BLOCK_SIZE, 0);
			} catch (...) { return false; }

			/* return base + offset in VM area */
			addr_t block_base = _base + (_index * BLOCK_SIZE);
			++_index;

			_range.add_range(block_base, BLOCK_SIZE);
			return true;
		}

	public:

		Slab_backend_alloc(Genode::Env &env, Genode::Ram_allocator &ram,
		                   Genode::Region_map &rm, Genode::Allocator &md_alloc)
		:
			Rm_connection(env),
			Region_map_client(Rm_connection::create(VM_SIZE)),
			_index(0), _range(&md_alloc), _ram(ram)
		{
			/* reserver attach us, anywere */
			_base = rm.attach(dataspace());
		}

		addr_t start() const { return _base; }
		addr_t end()   const { return _base + VM_SIZE - 1; }


		/*************************
		 ** Allocator interface **
		 *************************/

		bool alloc(Genode::size_t size, void **out_addr)
		{
			bool done = _range.alloc(size, out_addr);

			if (done)
				return done;

			done = _alloc_block();
			if (!done) {
				Genode::error("Backend allocator exhausted\n");
				return false;
			}

			return _range.alloc(size, out_addr);
		}

		void           free(void *addr, Genode::size_t size) { _range.free(addr, size); }
		Genode::size_t overhead(Genode::size_t size) const   { return  0; }
		bool           need_size_for_free() const override   { return false; }
};


/**
 * Slab allocator using our back-end allocator
 */
class Bsd::Slab_alloc : public Genode::Slab
{
	private:

		Genode::size_t const _object_size;

		static Genode::size_t _calculate_block_size(Genode::size_t object_size)
		{
			Genode::size_t const block_size = 8*object_size;
			return Genode::align_addr(block_size, 12);
		}

	public:

		Slab_alloc(Genode::size_t object_size, Slab_backend_alloc &allocator)
		:
			Slab(object_size, _calculate_block_size(object_size), 0, &allocator),
			_object_size(object_size)
		{ }

		Genode::addr_t alloc()
		{
			Genode::addr_t result;
			return (Slab::alloc(_object_size, (void **)&result) ? result : 0);
		}

		void free(void *ptr) { Slab::free(ptr, _object_size); }
};


/**
 * Memory interface
 */
class Bsd::Malloc
{
	private:

		enum {
			SLAB_START_LOG2 = 5,  /* 32 B */
			SLAB_STOP_LOG2  = 17, /* 128 KiB */
			NUM_SLABS = (SLAB_STOP_LOG2 - SLAB_START_LOG2) + 1,
		};

		typedef Genode::addr_t          addr_t;
		typedef Bsd::Slab_alloc         Slab_alloc;
		typedef Bsd::Slab_backend_alloc Slab_backend_alloc;

		Slab_backend_alloc      &_back_allocator;
		Slab_alloc              *_allocator[NUM_SLABS];
		addr_t                   _start;
		addr_t                   _end;

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
		Genode::size_t _get_orig_size(Genode::addr_t **addr)
		{
			using namespace Genode;

			addr_t index = *(*addr - 1);
			if (index > 32) {
				*addr = (addr_t *) * (*addr - 1);
			}

			return *(*addr - 2);
		}

	public:

		Malloc(Slab_backend_alloc &alloc, Genode::Allocator &md_alloc)
		:
			_back_allocator(alloc), _start(alloc.start()),
			_end(alloc.end())
		{
			/* init slab allocators */
			for (unsigned i = SLAB_START_LOG2; i <= SLAB_STOP_LOG2; i++) {
				_allocator[i - SLAB_START_LOG2] =
					new (&md_alloc) Slab_alloc(1U << i, _back_allocator);
			}
		}

		/**
		 * Alloc in slabs
		 */
		void *alloc(Genode::size_t size, int align = 0)
		{
			using namespace Genode;

			/* save requested size */
			Genode::size_t orig_size = size;
			size += sizeof(addr_t);

			/* += slab index + aligment size */
			size += sizeof(addr_t) + (align > 2 ? (1 << align) : 0);

			int msb = Genode::log2(size);

			if (size > (1U << msb))
				msb++;

			if (size < (1U << SLAB_START_LOG2))
				msb = SLAB_STOP_LOG2;

			if (msb > SLAB_STOP_LOG2) {
				Genode::error("Slab too large ", 1U << msb, " reqested ", size);
				return 0;
			}

			addr_t addr =  _allocator[msb - SLAB_START_LOG2]->alloc();
			if (!addr) {
				Genode::error("Failed to get slab for ", 1U << msb);
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

			return (addr_t *)addr;
		}

		void free(void const *a)
		{
			using namespace Genode;
			addr_t *addr = (addr_t *)a;

			unsigned nr = _slab_index(&addr);
			/* we need to decrease addr by 2, orig_size and index come first */
			_allocator[nr]->free((void *)(addr - 2));
		}

		Genode::size_t size(void const *a)
		{
			using namespace Genode;
			addr_t *addr = (addr_t *)a;

			return _get_orig_size(&addr);
		}

		bool inside(addr_t const addr) const { return (addr > _start) && (addr <= _end); }
};


static Bsd::Malloc *_malloc;


void Bsd::mem_init(Genode::Env &env, Genode::Allocator &alloc)
{
	static Bsd::Slab_backend_alloc sb(env, env.ram(), env.rm(), alloc);
	static Bsd::Malloc m(sb, alloc);
	_malloc = &m;
}


static Bsd::Malloc& malloc_backend() { return *_malloc; }


/**********************
 ** Memory allocation *
 **********************/

extern "C" void *malloc(size_t size, int type, int flags)
{
	void *addr = malloc_backend().alloc(size);

	if (addr && (flags & M_ZERO))
		Genode::memset(addr, 0, size);

	return addr;
}


extern "C" void *mallocarray(size_t nmemb, size_t size, int type, int flags)
{
	if (size != 0 && nmemb > (~0UL / size))
		return 0;

	return malloc(nmemb * size, type, flags);
}


extern "C" void free(void *addr, int type, size_t size)
{
	if (!addr) return;

	if (!malloc_backend().inside((Genode::addr_t)addr)) {
		Genode::error("cannot free unknown memory at ", __builtin_return_address(0),
		              " called from ", addr);
		return;
	}

	if (size) {
		size_t ssize = malloc_backend().size(addr);

		if (ssize != size) {
			Genode::warning("size: ", size, "for ", addr,
			                " does not match stored size: ", ssize);
		}
	}

	malloc_backend().free(addr);
}


/*****************
 ** sys/systm.h **
 *****************/

extern "C" void bzero(void *b, size_t len)
{
	if (b == nullptr) return;

	Genode::memset(b, 0, len);
}


extern "C" void bcopy(const void *src, void *dst, size_t len)
{
	/* XXX may overlap */
	Genode::memcpy(dst, src, len);
}


extern "C" int uiomove(void *buf, int n, struct uio *uio)
{
	void *dst = nullptr;
	void *src = nullptr;
	size_t len = uio->uio_resid < (size_t)n ? uio->uio_resid : (size_t)n;

	switch (uio->uio_rw) {
	case UIO_READ:
		dst = buf;
		src = ((char*)uio->buf) + uio->uio_offset;
		break;
	case UIO_WRITE:
		dst = ((char*)uio->buf) + uio->uio_offset;
		src = buf;
		break;
	}

	Genode::memcpy(dst, src, len);

	uio->uio_resid  -= len;
	uio->uio_offset += len;

	return 0;
}
