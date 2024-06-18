/*
 * \brief  hw-specific implementation of core-local RM session
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2014-02-26
 *
 * Taken from OKL4-specific imlementation
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <core_region_map.h>
#include <map_local.h>
#include <util.h>

using namespace Core;


Region_map::Attach_result
Core_region_map::attach(Dataspace_capability ds_cap, Attr const &attr)
{
	return _ep.apply(ds_cap, [&] (Dataspace_component *ds_ptr) -> Attach_result {

		if (!ds_ptr)
			return Attach_error::INVALID_DATASPACE;

		Dataspace_component &ds = *ds_ptr;

		size_t const size = (attr.size == 0) ? ds.size() : attr.size;
		size_t const page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

		/* attach attributes 'use_at' and 'offset' not supported within core */
		if (attr.use_at || attr.offset)
			return Attach_error::REGION_CONFLICT;

		unsigned const align = get_page_size_log2();

		/* allocate range in core's virtual address space */
		Allocator::Alloc_result const virt =
			platform().region_alloc().alloc_aligned(page_rounded_size, align);

		if (virt.failed()) {
			error("could not allocate virtual address range in core of size ",
			      page_rounded_size);
			return Attach_error::REGION_CONFLICT;
		}

		using namespace Hw;

		/* map the dataspace's physical pages to corresponding virtual addresses */
		unsigned const num_pages = unsigned(page_rounded_size >> get_page_size_log2());

		Page_flags const flags {
			.writeable  = (attr.writeable && ds.writeable()) ? RW : RO,
			.executable = NO_EXEC,
			.privileged = KERN,
			.global     = GLOBAL,
			.type       = ds.io_mem() ? DEVICE : RAM,
			.cacheable  = ds.cacheability()
		};

		return virt.convert<Attach_result>(

			[&] (void *virt_addr) -> Attach_result {
				if (map_local(ds.phys_addr(), (addr_t)virt_addr, num_pages, flags))
					return Range { .start     = addr_t(virt_addr),
					               .num_bytes = page_rounded_size };

				platform().region_alloc().free(virt_addr, page_rounded_size);
				return Attach_error::REGION_CONFLICT; },

			[&] (Allocator::Alloc_error) {
				return Attach_error::REGION_CONFLICT; });
	});
}


void Core_region_map::detach(addr_t) { }
