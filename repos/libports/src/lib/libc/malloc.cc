/*
 * \brief  Simplistic malloc and free implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2006-07-21
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <base/slab.h>
#include <util/construct_at.h>
#include <util/string.h>
#include <util/misc_math.h>

/* libc includes */
extern "C" {
#include <string.h>
#include <stdlib.h>
}

typedef unsigned long Block_header;

namespace Genode {

	class Slab_alloc : public Slab
	{
		private:

			size_t const _object_size;

			size_t _calculate_block_size(size_t object_size)
			{
				size_t block_size = 16*object_size;
				return align_addr(block_size, 12);
			}

		public:

			Slab_alloc(size_t object_size, Allocator *backing_store)
			:
				Slab(object_size, _calculate_block_size(object_size), 0, backing_store),
				_object_size(object_size)
			{ }

			void *alloc()
			{
				void *result;
				return (Slab::alloc(_object_size, &result) ? result : 0);
			}

			void free(void *ptr) { Slab::free(ptr, _object_size); }
	};
}


/**
 * Allocator that uses slabs for small objects sizes
 */
class Malloc : public Genode::Allocator
{
	private:

		typedef Genode::size_t size_t;

		enum {
			SLAB_START = 2,  /* 4 Byte (log2) */
			SLAB_STOP  = 11, /* 2048 Byte (log2) */
			NUM_SLABS = (SLAB_STOP - SLAB_START) + 1
		};

		Genode::Allocator  *_backing_store;        /* back-end allocator */
		Genode::Slab_alloc *_allocator[NUM_SLABS]; /* slab allocators */
		Genode::Lock        _lock;

		unsigned long _slab_log2(unsigned long size) const
		{
			unsigned msb = Genode::log2(size);
			/* size is greater than msb */
			if (size > (1U << msb))
				msb++;

			/* use smallest slab */
			if (size < (1U << SLAB_START))
				msb = SLAB_START;

			return msb;
		}

	public:

		Malloc(Genode::Allocator *backing_store) : _backing_store(backing_store)
		{
			for (unsigned i = SLAB_START; i <= SLAB_STOP; i++) {
				_allocator[i - SLAB_START] = new (backing_store)
				                                 Genode::Slab_alloc(1U << i, backing_store);
			}
		}

		~Malloc() { Genode::warning(__func__, " unexpectedly called"); }

		/**
		 * Allocator interface
		 */

		bool alloc(size_t size, void **out_addr) override
		{
			Genode::Lock::Guard lock_guard(_lock);

			/* enforce size to be a multiple of 4 bytes */
			size = (size + 3) & ~3;

			/*
			 * We store the size of the allocation at the very
			 * beginning of the allocated block and return
			 * the subsequent address. This way, we can retrieve
			 * the size information when freeing the block.
			 */
			unsigned long real_size = size + sizeof(Block_header);
			unsigned long msb = _slab_log2(real_size);
			void *addr = 0;

			/* use backing store if requested memory is larger than largest slab */
			if (msb > SLAB_STOP) {

				if (!(_backing_store->alloc(real_size, &addr)))
					return false;
			}
			else
				if (!(addr = _allocator[msb - SLAB_START]->alloc()))
					return false;

			*(Block_header *)addr = real_size;
			*out_addr = (Block_header *)addr + 1;
			return true;
		}

		void free(void *ptr, size_t /* size */) override
		{
			Genode::Lock::Guard lock_guard(_lock);

			unsigned long *addr = ((unsigned long *)ptr) - 1;
			unsigned long  real_size = *addr;

			if (real_size > (1U << SLAB_STOP))
				_backing_store->free(addr, real_size);
			else {
				unsigned long msb = _slab_log2(real_size);
				_allocator[msb - SLAB_START]->free(addr);
			}
		}

		size_t overhead(size_t size) const override
		{
			size += sizeof(Block_header);

			if (size > (1U << SLAB_STOP))
				return _backing_store->overhead(size);

			return _allocator[_slab_log2(size) - SLAB_START]->overhead(size);
		}

		bool need_size_for_free() const override { return false; }
};


static Genode::Allocator *allocator()
{
	static bool constructed = 0;
	static char placeholder[sizeof(Malloc)];
	if (!constructed) {
		Genode::construct_at<Malloc>(placeholder, Genode::env()->heap());
		constructed = 1;
	}

	return reinterpret_cast<Malloc *>(placeholder);
}


extern "C" void *malloc(size_t size)
{
	void *addr;
	return allocator()->alloc(size, &addr) ? addr : 0;
}


extern "C" void *calloc(size_t nmemb, size_t size)
{
	void *addr = malloc(nmemb*size);
	if (addr)
		Genode::memset(addr, 0, nmemb*size);
	return addr;
}


extern "C" void free(void *ptr)
{
	if (!ptr) return;

	allocator()->free(ptr, 0);
}


extern "C" void *realloc(void *ptr, size_t size)
{
	if (!ptr)
		return malloc(size);

	if (!size) {
		free(ptr);
		return 0;
	}

	/* determine size of old block content (without header) */
	unsigned long old_size = *((Block_header *)ptr - 1)
	                         - sizeof(Block_header);

	/* do not reallocate if new size is less than the current size */
	if (size <= old_size)
		return ptr;

	/* allocate new block */
	void *new_addr = malloc(size);

	/* copy content from old block into new block */
	if (new_addr)
		memcpy(new_addr, ptr, Genode::min(old_size, (unsigned long)size));

	/* free old block */
	free(ptr);

	return new_addr;
}
