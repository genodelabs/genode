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
#include <base/heap.h>

using namespace Genode;


Region_map::Local_addr
Core_region_map::attach(Dataspace_capability ds_cap, size_t size,
                        off_t offset, bool use_local_addr,
                        Region_map::Local_addr, bool, bool writeable)
{
	return _ep.apply(ds_cap, [&] (Dataspace_component *ds_ptr) -> Local_addr {

		if (!ds_ptr)
			throw Invalid_dataspace();

		Dataspace_component &ds = *ds_ptr;

		if (size == 0)
			size = ds.size();

		size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

		if (use_local_addr) {
			error("Parameter 'use_local_addr' not supported within core");
			return nullptr;
		}

		if (offset) {
			error("Parameter 'offset' not supported within core");
			return nullptr;
		}

		unsigned const align = get_page_size_log2();

		/* allocate range in core's virtual address space */
		Allocator::Alloc_result virt =
			platform().region_alloc().alloc_aligned(page_rounded_size, align);

		if (virt.failed()) {
			error("could not allocate virtual address range in core of size ",
			      page_rounded_size);
			return nullptr;
		}

		using namespace Hw;

		/* map the dataspace's physical pages to corresponding virtual addresses */
		unsigned num_pages = (unsigned)(page_rounded_size >> get_page_size_log2());
		Page_flags const flags { (writeable && ds.writeable()) ? RW : RO,
		                         NO_EXEC, KERN, GLOBAL,
		                         ds.io_mem() ? DEVICE : RAM,
		                         ds.cacheability() };

		return virt.convert<Local_addr>(

			[&] (void *virt_addr) -> void * {
				if (map_local(ds.phys_addr(), (addr_t)virt_addr, num_pages, flags))
					return virt_addr;

				platform().region_alloc().free(virt_addr, page_rounded_size);
				return nullptr; },

			[&] (Allocator::Alloc_error) {
				return nullptr; });
	});
}


void Core_region_map::detach(Local_addr) { }
