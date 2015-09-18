/*
 * \brief  Linux kernel memory allocator
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__MALLOC_H_
#define _LX_EMUL__IMPL__INTERNAL__MALLOC_H_

#include <lx_emul/impl/internal/slab_alloc.h>
#include <lx_emul/impl/internal/slab_backend_alloc.h>

namespace Lx { class Malloc; }


class Lx::Malloc
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
				PERR("Slab too large %u reqested %zu cached %d", 1U << msb, size, _cached);
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

#endif /* _LX_EMUL__IMPL__INTERNAL__MALLOC_H_ */
