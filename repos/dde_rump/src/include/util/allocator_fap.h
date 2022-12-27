/**
 * \brief  Fast allocator for porting
 * \author Sebastian Sumpf
 * \date   2013-06-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__ALLOCATOR_FAP_H_
#define _INCLUDE__UTIL__ALLOCATOR_FAP_H_

#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <rump/env.h>

namespace Allocator {
	template <unsigned long VM_SIZE, typename POLICY> class Backend_alloc;
	template <unsigned long VM_SIZE, typename POLICY> class Fap;
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
	template <unsigned long VM_SIZE, typename POLICY = Default_allocator_policy>
	class Backend_alloc : public Genode::Allocator,
	                      public Genode::Rm_connection,
	                      public Genode::Region_map_client
	{
		private:

			enum {
				BLOCK_SIZE = 4 * 1024  * 1024,     /* bytes */
				ELEMENTS   = VM_SIZE / BLOCK_SIZE, /* MAX number of dataspaces in VM */
			};

			typedef Genode::addr_t addr_t;
			typedef Genode::Ram_dataspace_capability Ram_dataspace_capability;
			typedef Genode::Allocator_avl Allocator_avl;

			addr_t                   _base;              /* virt. base address */
			Cache                    _cache;             /* non-/cached RAM */
			Ram_dataspace_capability _ds_cap[ELEMENTS];  /* dataspaces to put in VM */
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
					_ds_cap[_index] =  Rump::env().env().ram().alloc(BLOCK_SIZE, _cache);
					/* attach at index * BLOCK_SIZE */
					Region_map_client::attach_at(_ds_cap[_index], _index * BLOCK_SIZE, BLOCK_SIZE, 0);
				} catch (Genode::Out_of_ram) {
					warning("backend allocator exhausted (out of RAM)");
					_quota_exceeded = true;
					return false;
				} catch (Genode::Out_of_caps) {
					warning("backend allocator exhausted (out of caps)");
					_quota_exceeded = true;
					return false;
				} catch (Genode::Region_map::Region_conflict) {
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

			Backend_alloc(Cache cache)
			:
				Rm_connection(Rump::env().env()),
				Region_map_client(Rm_connection::create(VM_SIZE)),
				_cache(cache),
				_range(&Rump::env().heap())
			{
				/* reserver attach us, anywere */
				_base = Rump::env().env().rm().attach(dataspace());
			}

			/**
			 * Allocate
			 */
			Alloc_result try_alloc(size_t size) override
			{
				Alloc_result result = _range.try_alloc(size);
				if (result.ok())
					return result;

				if (!_alloc_block())
					return Alloc_error::DENIED;

				return _range.try_alloc(size);
			}

			void *alloc_aligned(size_t size, unsigned align = 0)
			{
				Alloc_result result = _range.alloc_aligned(size, align);
				if (result.ok())
					return result.convert<void *>(
						[&] (void *ptr) { return ptr; },
						[&] (Alloc_error) -> void * { return nullptr; });

				if (!_alloc_block())
					return 0;

				return _range.alloc_aligned(size, align).convert<void *>(

					[&] (void *ptr) {
						return ptr; },

					[&] (Alloc_error e) -> void * {
						error("backend allocator: Unable to allocate memory "
						      "(size: ", size, " align: ", align, ")");
						return nullptr;
					}
				);
			}

			void   free(void *addr, size_t size) override { _range.free(addr, size); }
			size_t overhead(size_t size) const override { return  0; }
			bool need_size_for_free() const override { return false; }

			bool inside(addr_t addr) const { return (addr >= _base) && (addr < (_base + VM_SIZE)); }
	};


	/**
	 * Interface
	 */
	template <unsigned long VM_SIZE, typename POLICY = Default_allocator_policy>
	class Fap
	{
		private:

			typedef Allocator::Backend_alloc<VM_SIZE, POLICY> Backend_alloc;

			Backend_alloc  _back_allocator;

		public:

			Fap(bool cache) : _back_allocator(cache ? CACHED : UNCACHED) { }

			void *alloc(size_t size, unsigned align = 0)
			{
				return _back_allocator.alloc_aligned(size, align);
			}

			void free(void *addr, size_t size)
			{
				_back_allocator.free(addr, size);
			}
	};
} /* namespace Allocator */

#endif /* _INCLUDE__UTIL__ALLOCATOR_FAP_H_ */
