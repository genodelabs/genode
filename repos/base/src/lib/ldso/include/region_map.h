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

namespace Linker { class Region_map; }


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

	protected:

		Region_map(Env &env, Allocator &md_alloc, addr_t base)
		:
			_env(env), _range(&md_alloc),
			_base((addr_t)_env.rm().attach_at(_rm.dataspace(), base))
		{
			_range.add_range(base, Pd_session::LINKER_AREA_SIZE);
		}

	public:

		typedef Constructible<Region_map> Constructible_region_map;

		static Constructible_region_map &r();

		/**
		 * Reserve VM region of 'size' at 'vaddr'. Allocate any free region if
		 * 'vaddr' is zero
		 */
		addr_t alloc_region(size_t size, addr_t vaddr = 0)
		{
			addr_t addr = vaddr;

			if (addr && (_range.alloc_addr(size, addr).error()))
				throw Region_conflict();
			else if (!addr &&
			         _range.alloc_aligned(size, (void **)&addr,
			                              get_page_size_log2()).error())
			{
				throw Region_conflict();
			}

			return addr;
		}

		void free_region(addr_t vaddr) { _range.free((void *)vaddr); }

		/**
		 * Overwritten from 'Region_map_client'
		 */
		Local_addr attach_at(Dataspace_capability ds, addr_t local_addr,
		                     size_t size = 0, off_t offset = 0)
		{
			return retry<Genode::Region_map::Out_of_metadata>(
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
			return retry<Genode::Region_map::Out_of_metadata>(
				[&] () {
					return _rm.attach_executable(ds, local_addr - _base, size, offset);
				},
				[&] () { _env.upgrade(Parent::Env::pd(), "ram_quota=8K"); });
		}

		void detach(Local_addr local_addr) { _rm.detach((addr_t)local_addr - _base); }
};

#endif /* _INCLUDE__REGION_MAP_H_ */
