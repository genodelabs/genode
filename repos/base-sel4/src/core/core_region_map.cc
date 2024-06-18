/*
 * \brief  Core-local region map
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <core_region_map.h>
#include <platform.h>
#include <map_local.h>

using namespace Core;


Region_map::Attach_result
Core_region_map::attach(Dataspace_capability ds_cap, Attr const &attr)
{
	return _ep.apply(ds_cap, [&] (Dataspace_component *ds) -> Attach_result {
		if (!ds)
			return Attach_error::INVALID_DATASPACE;

		size_t const size = (attr.size == 0) ? ds->size() : attr.size;
		size_t const page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

		/* attach attributes 'use_at' and 'offset' not supported within core */
		if (attr.use_at || attr.offset)
			return Attach_error::REGION_CONFLICT;

		/* allocate range in core's virtual address space */
		return platform().region_alloc().try_alloc(page_rounded_size).convert<Attach_result>(
			[&] (void *virt_ptr) {

				/* map the dataspace's physical pages to core-local virtual addresses */
				size_t num_pages = page_rounded_size >> get_page_size_log2();
				map_local(ds->phys_addr(), (addr_t)virt_ptr, num_pages);

				return Range { .start = addr_t(virt_ptr), .num_bytes = page_rounded_size };
			},
			[&] (Range_allocator::Alloc_error) -> Attach_result {
				error("could not allocate virtual address range in core of size ",
				      page_rounded_size);
				return Attach_error::REGION_CONFLICT;
			}
		);
	});
}


void Core_region_map::detach(addr_t const at)
{
	size_t const size = platform_specific().region_alloc_size_at((void *)at);

	if (!unmap_local(at, size >> get_page_size_log2())) {
		error("could not unmap core virtual address ", Hex(at), " in ", __PRETTY_FUNCTION__);
		return;
	}

	platform().region_alloc().free((void *)at);
}
