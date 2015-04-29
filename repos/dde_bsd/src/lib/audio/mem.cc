/**
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-16
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <util/string.h>

/* local includes */
#include "bsd.h"
#include <extern_c_begin.h>
# include <bsd_emul.h>
#include <extern_c_end.h>


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
                                public Genode::Rm_connection
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
		Genode::Ram_session              &_ram;               /* ram session to allocate ds from */

		bool _alloc_block()
		{
			if (_index == ELEMENTS) {
				PERR("Slab-backend exhausted!");
				return false;
			}

			try {
				_ds_cap[_index] = _ram.alloc(BLOCK_SIZE);
				Rm_connection::attach_at(_ds_cap[_index], _index * BLOCK_SIZE, BLOCK_SIZE, 0);
			} catch (...) { return false; }

			/* return base + offset in VM area */
			addr_t block_base = _base + (_index * BLOCK_SIZE);
			++_index;

			_range.add_range(block_base, BLOCK_SIZE);
			return true;
		}

	public:

		Slab_backend_alloc(Genode::Ram_session &ram)
		:
			Rm_connection(0, VM_SIZE),
			_index(0), _range(Genode::env()->heap()), _ram(ram)
		{
			/* reserver attach us, anywere */
			_base = Genode::env()->rm_session()->attach(dataspace());
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
				PERR("Backend allocator exhausted\n");
				return false;
			}

			return _range.alloc(size, out_addr);
		}

		void           free(void *addr, Genode::size_t size) { }
		Genode::size_t overhead(Genode::size_t size) const   { return  0; }
		bool           need_size_for_free() const override   { return false; }
};


/**
 * Slab allocator using our back-end allocator
 */
class Bsd::Slab_alloc : public Genode::Slab
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
 * Memory interface
 */
class Bsd::Malloc
{
	private:

		enum {
			SLAB_START_LOG2 = 5,  /* 32 B */
			SLAB_STOP_LOG2  = 17, /* 64 KiB */
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

		Malloc(Slab_backend_alloc &alloc)
		:
			_back_allocator(alloc), _start(alloc.start()),
			_end(alloc.end())
		{
			/* init slab allocators */
			for (unsigned i = SLAB_START_LOG2; i <= SLAB_STOP_LOG2; i++)
				_allocator[i - SLAB_START_LOG2] = new (Genode::env()->heap())
				                                  Slab_alloc(1U << i, _back_allocator);
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
				PERR("Slab too large %u reqested %zu", 1U << msb, size);
				return 0;
			}

			addr_t addr =  _allocator[msb - SLAB_START_LOG2]->alloc();
			if (!addr) {
				PERR("Failed to get slab for %u", 1U << msb);
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


static Bsd::Malloc& malloc_backend()
{
	static Bsd::Slab_backend_alloc sb(*Genode::env()->ram_session());
	static Bsd::Malloc m(sb);
	return m;
}


/**********************
 ** Memory allocation *
 **********************/

extern "C" void *malloc(size_t size, int type, int flags)
{
	void *addr = malloc_backend().alloc(size);

	if (flags & M_ZERO)
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
		PERR("cannot free unknown memory at %p, called from %p",
		     addr, __builtin_return_address(0));
		return;
	}

	if (size) {
		size_t ssize = malloc_backend().size(addr);

		if (ssize != size)
			PWRN("size: %zu for %p does not match stored size: %zu",
			     size, addr, ssize);
	}

	malloc_backend().free(addr);
}


/*****************
 ** sys/systm.h **
 *****************/

extern "C" void bcopy(const void *src, void *dst, size_t len)
{
	/* XXX may overlap */
	Genode::memcpy(dst, src, len);
}


extern "C" int uiomovei(void *buf, int n, struct uio *uio)
{
	void *dst  = buf;
	void *src  = ((char*)uio->buf) + uio->uio_offset;
	size_t len = uio->uio_resid < (size_t)n ? uio->uio_resid : (size_t)n;

	Genode::memcpy(dst, src, len);

	uio->uio_resid  -= len;
	uio->uio_offset += len;

	return 0;
}
