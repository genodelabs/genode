/**
 * \brief  Fast allocator for porting
 * \author Sebastian Sumpf
 * \date   2013-06-12
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__ALLOCATOR_FAP_H_
#define _INCLUDE__UTIL__ALLOCATOR_FAP_H_

#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <region_map/client.h>


namespace Allocator {
	template <unsigned VM_SIZE, typename POLICY> class Backend_alloc;
	template <unsigned VM_SIZE, typename POLICY> class Fap;
}


namespace Allocator {

	using namespace Genode;
	using Genode::size_t;

	struct Default_allocator_policy
	{
		static int  block()      { return 0; }
		static void unblock(int) { }
	};

	template <typename POLICY>
	struct Policy_guard
	{
		int val;
		Policy_guard()  { val = POLICY::block(); }
		~Policy_guard() { POLICY::unblock(val); }
	};

	/**
	 * Back-end allocator for Genode's slab allocator
	 */
	template <unsigned VM_SIZE, typename POLICY = Default_allocator_policy>
	class Backend_alloc : public Genode::Allocator,
	                      public Genode::Rm_connection,
	                      public Genode::Region_map_client
	{
		private:

			enum {
				BLOCK_SIZE = 1024  * 1024,         /* 1 MB */
				ELEMENTS   = VM_SIZE / BLOCK_SIZE, /* MAX number of dataspaces in VM */
			};

			typedef Genode::addr_t addr_t;
			typedef Genode::Ram_dataspace_capability Ram_dataspace_capability;
			typedef Genode::Allocator_avl Allocator_avl;

			addr_t                   _base;              /* virt. base address */
			Cache_attribute          _cached;            /* non-/cached RAM */
			Ram_dataspace_capability _ds_cap[ELEMENTS];  /* dataspaces to put in VM */
			addr_t                   _ds_phys[ELEMENTS]; /* physical bases of dataspaces */
			int                      _index = 0;         /* current index in ds_cap */
			Allocator_avl            _range;             /* manage allocations */
			bool                     _quota_exceeded = false;

			bool _alloc_block()
			{
				if (_quota_exceeded)
					return false;

				if (_index == ELEMENTS) {
					error("slab backend exhausted!");
					return false;
				}

				Policy_guard<POLICY> guard;

				try {
					_ds_cap[_index] =  Genode::env()->ram_session()->alloc(BLOCK_SIZE, _cached);
					/* attach at index * BLOCK_SIZE */
					Region_map_client::attach_at(_ds_cap[_index], _index * BLOCK_SIZE, BLOCK_SIZE, 0);
					/* lookup phys. address */
					_ds_phys[_index] = Genode::Dataspace_client(_ds_cap[_index]).phys_addr();
				} catch (Genode::Ram_session::Quota_exceeded) {
					warning("backend allocator exhausted");
					_quota_exceeded = true;
					return false;
				} catch (Genode::Region_map::Attach_failed) {
					warning("backend VM region exhausted");
					_quota_exceeded = true;
					return false;
				}

				/* return base + offset in VM area */
				addr_t block_base = _base + (_index * BLOCK_SIZE);
				++_index;

				_range.add_range(block_base, BLOCK_SIZE);
				return true;
			}

		public:

			Backend_alloc(Cache_attribute cached)
			:
				Region_map_client(Rm_connection::create(VM_SIZE)),
				_cached(cached),
				_range(Genode::env()->heap())
			{
				/* reserver attach us, anywere */
				_base = Genode::env()->rm_session()->attach(dataspace());
			}

			/**
			 * Allocate
			 */
			bool alloc(size_t size, void **out_addr)
			{
				bool done = _range.alloc(size, out_addr);

				if (done)
					return done;

				done = _alloc_block();
				if (!done)
					return false;

				return _range.alloc(size, out_addr);
			}

			void *alloc_aligned(size_t size, int align = 0)
			{
				void *addr;

				if (!_range.alloc_aligned(size, &addr, align).error())
					return addr;

				if (!_alloc_block())
					return 0;

				if (_range.alloc_aligned(size, &addr, align).error()) {
					error("backend allocator: Unable to allocate memory "
					      "(size: ", size, " align: ", align, ")");
					return 0;
				}

				return addr;
			}

			void   free(void *addr, size_t size) override { _range.free(addr, size); }
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

			bool inside(addr_t addr) const { return (addr >= _base) && (addr < (_base + VM_SIZE)); }
	};


	/**
	 * Interface
	 */
	template <unsigned VM_SIZE, typename POLICY = Default_allocator_policy>
	class Fap
	{
		private:

			typedef Allocator::Backend_alloc<VM_SIZE, POLICY> Backend_alloc;

			Backend_alloc  _back_allocator;

		public:

			Fap(bool cached)
			: _back_allocator(cached ? CACHED : UNCACHED) { }

			void *alloc(size_t size, int align = 0)
			{
				return _back_allocator.alloc_aligned(size, align);
			}

			void free(void *addr, size_t size)
			{
				_back_allocator.free(addr, size);
			}

			addr_t phys_addr(void *addr)
			{
				return _back_allocator.phys_addr((addr_t)addr);
			}
	};
} /* namespace Allocator */

#endif /* _INCLUDE__UTIL__ALLOCATOR_FAP_H_ */
