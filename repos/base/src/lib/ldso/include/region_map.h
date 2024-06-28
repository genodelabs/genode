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
	private:

		Env              &_env;
		Region_map_client _rm { _env.pd().linker_area() };
		Allocator_avl     _range; /* VM range allocator */
		addr_t      const _base;  /* base address of dataspace */
		addr_t            _end = _base + Pd_session::LINKER_AREA_SIZE;

	protected:

		Region_map(Env &env, Allocator &md_alloc, addr_t base)
		:
			_env(env), _range(&md_alloc), _base(base)
		{
			_env.rm().attach(_rm.dataspace(), Genode::Region_map::Attr {
				.size       = 0,
				.offset     = 0,
				.use_at     = true,
				.at         = _base,
				.executable = true,
				.writeable  = true
			}).with_result(
				[&] (Genode::Region_map::Range) {
					_range.add_range(base, Pd_session::LINKER_AREA_SIZE);

					if (Linker::verbose)
						log("  ",   Hex(base),
						    " .. ", Hex(base + Pd_session::LINKER_AREA_SIZE - 1),
						    ": linker area");
				},
				[&] (Genode::Region_map::Attach_error) {
					error("failed to locally attach linker area"); }
			);
		}

	public:

		using Constructible_region_map = Constructible<Region_map>;

		static Constructible_region_map &r();

		using Alloc_region_error  = Ram_allocator::Alloc_error;
		using Alloc_region_result = Attempt<addr_t, Alloc_region_error>;
		using Attach_result       = Genode::Region_map::Attach_result;
		using Attr                = Genode::Region_map::Attr;

		/**
		 * Allocate region anywhere within the region map
		 */
		Alloc_region_result alloc_region(size_t size)
		{
			return _range.alloc_aligned(size, get_page_size_log2()).convert<Alloc_region_result>(
				[&] (void *ptr)                { return (addr_t)ptr; },
				[&] (Allocator::Alloc_error e) { return e; });
		}

		/**
		 * Allocate region at specified 'vaddr'
		 */
		Alloc_region_result alloc_region_at(size_t size, addr_t vaddr)
		{
			return _range.alloc_addr(size, vaddr).convert<Alloc_region_result>(
				[&] (void *ptr)                { return (addr_t)ptr; },
				[&] (Allocator::Alloc_error e) { return e; });
		}

		Alloc_region_result alloc_region_at_end(size_t size)
		{
			_end -= align_addr(size, get_page_size_log2());
			return alloc_region_at(size, _end);
		}

		void free_region(addr_t vaddr) { _range.free((void *)vaddr); }

		Attach_result attach(Dataspace_capability ds, Attr attr)
		{
			if (!attr.use_at)
				error("unexpected arguments of Linker::Region_map::attach");

			attr.at -= _base;
			return _rm.attach(ds, attr).convert<Attach_result>(
				[&] (Genode::Region_map::Range range) {
					range.start += _base;
					return range;
				},
				[&] (Genode::Region_map::Attach_error e) { return e; }
			);
		}

		void detach(addr_t local_addr) { _rm.detach(local_addr - _base); }
};

#endif /* _INCLUDE__REGION_MAP_H_ */
