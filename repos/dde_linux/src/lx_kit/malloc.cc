/*
 * \brief  Linux kit memory allocator
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/slab.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

/* Linux kit includes */
#include <lx_kit/types.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/malloc.h>


namespace Lx_kit {
	class Slab_backend_alloc;
	class Slab_alloc;
	class Malloc;
}


class Lx_kit::Slab_backend_alloc : public Lx::Slab_backend_alloc,
                                   public Genode::Rm_connection,
                                   public Genode::Region_map_client
{
	private:

		enum {
			VM_SIZE    = 64 * 1024 * 1024,       /* size of VM region to reserve */
			P_BLOCK_SIZE  = 2 * 1024 * 1024,     /* 2 MB physical contiguous */
			V_BLOCK_SIZE  = P_BLOCK_SIZE * 2,    /* 2 MB virtual used, 2 MB virtual left free to avoid that Allocator_avl merges virtual contiguous regions which are physically non-contiguous */
			ELEMENTS   = VM_SIZE / V_BLOCK_SIZE, /* MAX number of dataspaces in VM */
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
				Genode::error("slab backend exhausted!");
				return false;
			}

			try {
				_ds_cap[_index] = Lx::backend_alloc(P_BLOCK_SIZE, _cached);
				/* attach at index * V_BLOCK_SIZE */
				Region_map_client::attach_at(_ds_cap[_index], _index * V_BLOCK_SIZE, P_BLOCK_SIZE, 0);

				/* lookup phys. address */
				_ds_phys[_index] = Genode::Dataspace_client(_ds_cap[_index]).phys_addr();
			} catch (...) { return false; }

			/* return base + offset in VM area */
			addr_t block_base = _base + (_index * V_BLOCK_SIZE);
			++_index;

			_range.add_range(block_base, P_BLOCK_SIZE);
			return true;
		}

	public:

		Slab_backend_alloc(Genode::Cache_attribute cached)
		:
			Region_map_client(Rm_connection::create(VM_SIZE)),
			_cached(cached), _index(0), _range(Genode::env()->heap())
		{
			/* reserver attach us, anywere */
			_base = Genode::env()->rm_session()->attach(dataspace());
		}

		static Slab_backend_alloc &mem()
		{
			static Lx_kit::Slab_backend_alloc inst(Genode::CACHED);
			return inst;
		}

		static Slab_backend_alloc &dma()
		{
			static Lx_kit::Slab_backend_alloc inst(Genode::UNCACHED);
			return inst;
		}

		/**************************************
		 ** Lx::Slab_backend_alloc interface **
		 **************************************/

		bool alloc(size_t size, void **out_addr) override
		{
			bool done = _range.alloc(size, out_addr);

			if (done)
				return done;

			done = _alloc_block();
			if (!done) {
				Genode::error("backend allocator exhausted");
				return false;
			}

			return _range.alloc(size, out_addr);
		}

		void free(void *addr) {
			_range.free(addr); }

		void   free(void *addr, size_t size) override { _range.free(addr, size); }
		size_t overhead(size_t size) const override { return  0; }
		bool need_size_for_free() const override { return false; }

		addr_t phys_addr(addr_t addr)
		{
			if (addr < _base || addr >= (_base + VM_SIZE))
				return ~0UL;

			int index = (addr - _base) / V_BLOCK_SIZE;

			/* physical base of dataspace */
			addr_t phys = _ds_phys[index];

			if (!phys)
				return ~0UL;

			/* add offset */
			phys += (addr - _base - (index * V_BLOCK_SIZE));
			return phys;
		}

		addr_t virt_addr(addr_t phys)
		{
			for (unsigned i = 0; i < ELEMENTS; i++) {
				if (_ds_cap[i].valid() &&
				    phys >= _ds_phys[i] && phys < _ds_phys[i] + P_BLOCK_SIZE)
					return _base +  i * V_BLOCK_SIZE + phys - _ds_phys[i];
			}

			Genode::warning("virt_addr(", Genode::Hex(phys), ") - no translation");
			return 0;
		}

		addr_t start() const { return _base; }
		addr_t end()   const { return _base + VM_SIZE - 1; }
};


class Lx_kit::Malloc : public Lx::Malloc
{
	private:

		enum {
			SLAB_START_LOG2 = 3,  /* 8 B */
			SLAB_STOP_LOG2  = MAX_SIZE_LOG2,
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

		static Malloc & mem()
		{
			static Malloc inst(Slab_backend_alloc::mem(), Genode::CACHED);
			return inst;
		}

		static Malloc & dma()
		{
			static Malloc inst(Slab_backend_alloc::dma(), Genode::UNCACHED);
			return inst;
		}

		/**************************
		 ** Lx::Malloc interface **
		 **************************/

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
				Genode::error("slab too large ",
				              1UL << msb, "reqested ", size, " cached ", (int)_cached);
				return 0;
			}

			addr_t addr =  _allocator[msb - SLAB_START_LOG2]->alloc();
			if (!addr) {
				Genode::error("failed to get slab for ", 1 << msb);
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

		void *alloc_large(size_t size)
		{
			void *addr;
			if (!_back_allocator.alloc(size, &addr)) {
				Genode::error("large back end allocation failed (", size, " bytes)");
				return nullptr;
			}

			return addr;
		}

		void free_large(void *ptr)
		{
			_back_allocator.free(ptr);
		}


		size_t size(void const *a)
		{
			using namespace Genode;
			addr_t *addr = (addr_t *)a;

			/* XXX changes addr */
			return _get_orig_size(&addr);
		}

		Genode::addr_t phys_addr(void *a) {
			return _back_allocator.phys_addr((addr_t)a); }

		Genode::addr_t virt_addr(Genode::addr_t phys) {
			return _back_allocator.virt_addr(phys); }

		bool inside(addr_t const addr) const { return (addr > _start) && (addr <= _end); }
};


/*******************************
 ** Lx::Malloc implementation **
 *******************************/

/**
 * Cached memory backend allocator
 */
Lx::Slab_backend_alloc &Lx::Slab_backend_alloc::mem() {
	return Lx_kit::Slab_backend_alloc::mem(); }


/**
 * DMA memory backend allocator
 */
Lx::Slab_backend_alloc &Lx::Slab_backend_alloc::dma() {
	return Lx_kit::Slab_backend_alloc::dma(); }


/**
 * Cached memory allocator
 */
Lx::Malloc &Lx::Malloc::mem() {
	return Lx_kit::Malloc::mem(); }

/**
 * DMA memory allocator
 */
Lx::Malloc &Lx::Malloc::dma() {
	return Lx_kit::Malloc::dma(); }

