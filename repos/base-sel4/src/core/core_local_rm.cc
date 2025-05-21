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
#include <core_local_rm.h>
#include <platform.h>
#include <map_local.h>
#include <dataspace_component.h>

using namespace Core;


Core_local_rm::Result
Core_local_rm::attach(Dataspace_capability ds_cap, Attach_attr const &attr)
{
	return _ep.apply(ds_cap, [&] (Dataspace_component *ds) -> Result {
		if (!ds)
			return Error::INVALID_DATASPACE;

		size_t const size = (attr.size == 0) ? ds->size() : attr.size;
		size_t const page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

		/* attach attributes 'use_at' and 'offset' not supported within core */
		if (attr.use_at || attr.offset)
			return Error::REGION_CONFLICT;

		/* allocate range in core's virtual address space */
		return platform().region_alloc().try_alloc(page_rounded_size).convert<Result>(
			[&] (Range_allocator::Allocation &virt) -> Result {

				/* map the dataspace's physical pages to core-local virtual addresses */
				size_t num_pages = page_rounded_size >> get_page_size_log2();
				if (!map_local(ds->phys_addr(), (addr_t)virt.ptr, num_pages))
					return Error::REGION_CONFLICT;

				virt.deallocate = false;
				return { *this, { .ptr       = virt.ptr,
				                  .num_bytes = page_rounded_size } };
			},
			[&] (Alloc_error) -> Result {
				error("could not allocate virtual address range in core of size ",
				      page_rounded_size);
				return Error::REGION_CONFLICT;
			}
		);
	});
}


void Core_local_rm::_free(Attachment &virt)
{
	size_t const size = platform_specific().region_alloc_size_at(virt.ptr);

	if (!unmap_local(addr_t(virt.ptr), size >> get_page_size_log2())) {
		error("could not unmap core virtual address ", virt.ptr, " in ", __PRETTY_FUNCTION__);
		return;
	}

	platform().region_alloc().free(virt.ptr);
}
