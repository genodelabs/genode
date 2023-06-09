/*
 * \brief  Linker area
 * \author Sebastian Sumpf
 * \author Norman Feske
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGION_MAP_H_
#define _INCLUDE__REGION_MAP_H_

/* Genode includes */
#include <base/env.h>
#include <region_map/client.h>
#include <base/allocator_avl.h>
#include <util/retry.h>
#include <util/reconstructible.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* local includes */
#include <linker.h>

namespace Linker {
	extern bool verbose;
	class Region_map;
}


/**
 * Managed dataspace for ELF objects (singelton)
 */
class Linker::Region_map
{
	public:

		typedef Region_map_client::Local_addr      Local_addr;
		typedef Region_map_client::Region_conflict Region_conflict;

	private:

		Env              &_env;
		Region_map_client _rm { _env.pd().linker_area() };
		Allocator_avl     _range; /* VM range allocator */
		addr_t      const _base;  /* base address of dataspace */
		addr_t            _end = _base + Pd_session::LINKER_AREA_SIZE;

	protected:

		Region_map(Env &env, Allocator &md_alloc, addr_t base)
		:
			_env(env), _range(&md_alloc),
			_base((addr_t)_env.rm().attach_rwx(_rm.dataspace(), base))
		{
			_range.add_range(base, Pd_session::LINKER_AREA_SIZE);

			if (Linker::verbose)
				log("  ",   Hex(base),
				    " .. ", Hex(base + Pd_session::LINKER_AREA_SIZE - 1),
				    ": linker area");
		}

	public:

		typedef Constructible<Region_map> Constructible_region_map;

		static Constructible_region_map &r();

		/**
		 * Allocate region anywhere within the region map
		 *
		 * XXX propagate OUT_OF_RAM, OUT_OF_CAPS
		 */
		addr_t alloc_region(size_t size)
		{
			return _range.alloc_aligned(size, get_page_size_log2()).convert<addr_t>(
				[&] (void *ptr)                        { return (addr_t)ptr; },
				[&] (Allocator::Alloc_error) -> addr_t { throw Region_conflict(); });
		}

		/**
		 * Allocate region at specified 'vaddr'
		 */
		void alloc_region_at(size_t size, addr_t vaddr)
		{
			if (_range.alloc_addr(size, vaddr).failed())
				throw Region_conflict();
		}

		addr_t alloc_region_at_end(size_t size)
		{
			_end -= align_addr(size, get_page_size_log2());
			alloc_region_at(size, _end);
			return _end;
		}

		void free_region(addr_t vaddr) { _range.free((void *)vaddr); }

		/**
		 * Overwritten from 'Region_map_client'
		 */
		Local_addr attach_at(Dataspace_capability ds, addr_t local_addr,
		                     size_t size = 0, off_t offset = 0)
		{
			return retry<Genode::Out_of_ram>(
				[&] () {
					return _rm.attach_at(ds, local_addr - _base, size, offset);
				},
				[&] () { _env.upgrade(Parent::Env::pd(), "ram_quota=8K"); });
		}

		/**
		 * Overwritten from 'Region_map_client'
		 */
		Local_addr attach_executable(Dataspace_capability ds, addr_t local_addr,
		                             size_t size = 0, off_t offset = 0)
		{
			return retry<Genode::Out_of_ram>(
				[&] () {
					return _rm.attach_executable(ds, local_addr - _base, size, offset);
				},
				[&] () { _env.upgrade(Parent::Env::pd(), "ram_quota=8K"); });
		}

		void detach(Local_addr local_addr) { _rm.detach((addr_t)local_addr - _base); }
};

#endif /* _INCLUDE__REGION_MAP_H_ */
