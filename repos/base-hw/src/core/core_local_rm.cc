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
#include <core_local_rm.h>
#include <map_local.h>
#include <util.h>
#include <dataspace_component.h>

using namespace Core;


Core_local_rm::Result
Core_local_rm::attach(Dataspace_capability ds_cap, Attach_attr const &attr)
{
	using Virt_allocation = Range_allocator::Allocation;

	return _ep.apply(ds_cap, [&] (Dataspace_component *ds_ptr) -> Result {

		if (!ds_ptr)
			return Error::INVALID_DATASPACE;

		Dataspace_component &ds = *ds_ptr;

		size_t const size = (attr.size == 0) ? ds.size() : attr.size;
		size_t const page_rounded_size = (size + PAGE_SIZE - 1) & PAGE_MASK;

		/* attach attributes 'use_at' and 'offset' not supported within core */
		if (attr.use_at || attr.offset)
			return Error::REGION_CONFLICT;

		/* allocate range in core's virtual address space */
		Allocator::Alloc_result virt =
			platform().region_alloc().alloc_aligned(page_rounded_size, AT_PAGE);

		if (virt.failed()) {
			error("could not allocate virtual address range in core of size ",
			      page_rounded_size);
			return Error::REGION_CONFLICT;
		}

		using namespace Hw;

		/* map the dataspace's physical pages to corresponding virtual addresses */
		unsigned const num_pages = unsigned(page_rounded_size >> PAGE_SIZE_LOG2);

		Page_flags const flags {
			.writeable  = (attr.writeable && ds.writeable()) ? RW : RO,
			.executable = NO_EXEC,
			.privileged = KERN,
			.global     = GLOBAL,
			.type       = ds.io_mem() ? DEVICE : RAM,
			.cacheable  = ds.cacheability()
		};

		return virt.convert<Result>(

			[&] (Virt_allocation &a) -> Result {
				if (!map_local(ds.phys_addr(), (addr_t)a.ptr, num_pages, flags))
					return Error::REGION_CONFLICT;

				a.deallocate = false;
				return { *this, { .ptr       = a.ptr,
				                  .num_bytes = page_rounded_size } };
			},
			[&] (Alloc_error) {
				return Error::REGION_CONFLICT; });
	});
}


void Core_local_rm::_free(Attachment &a)
{
	size_t size = platform_specific().region_alloc_size_at(a.ptr);

	unmap_local(addr_t(a.ptr), size >> PAGE_SIZE_LOG2);

	platform().region_alloc().free(a.ptr);
}
