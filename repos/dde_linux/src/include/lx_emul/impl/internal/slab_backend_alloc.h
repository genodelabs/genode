
/*
 * \brief  Back-end allocator for Genode's slab allocator
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

#ifndef _LX_EMUL__IMPL__INTERNAL__SLAB_BACKEND_ALLOC_H_
#define _LX_EMUL__IMPL__INTERNAL__SLAB_BACKEND_ALLOC_H_

/* Genode includes */
#include <base/env.h>
#include <base/allocator_avl.h>
#include <rm_session/connection.h>
#include <dataspace/client.h>

namespace Lx {

	Genode::Ram_dataspace_capability
	backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached);

	void backend_free(Genode::Ram_dataspace_capability cap);
	class Slab_backend_alloc;
}

class Lx::Slab_backend_alloc : public Genode::Allocator,
                               public Genode::Rm_connection
{
	private:

		typedef Genode::addr_t addr_t;

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

#endif /* _LX_EMUL__IMPL__INTERNAL__SLAB_BACKEND_ALLOC_H_ */
