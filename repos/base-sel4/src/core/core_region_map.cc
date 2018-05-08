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

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <core_region_map.h>
#include <platform.h>
#include <map_local.h>

using namespace Genode;


Region_map::Local_addr
Core_region_map::attach(Dataspace_capability ds_cap, size_t size, off_t offset,
                        bool use_local_addr, Region_map::Local_addr, bool, bool)
{
	auto lambda = [&] (Dataspace_component *ds) -> Local_addr {
		if (!ds)
			throw Invalid_dataspace();

		if (size == 0)
			size = ds->size();

		size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

		if (use_local_addr) {
			error(__func__, ": 'use_local_addr' not supported within core");
			return nullptr;
		}

		if (offset) {
			error(__func__, ": 'offset' not supported within core");
			return nullptr;
		}

		/* allocate range in core's virtual address space */
		void *virt_addr;
		if (!platform()->region_alloc()->alloc(page_rounded_size, &virt_addr)) {
			error("could not allocate virtual address range in core of size ",
			      page_rounded_size);
			return nullptr;
		}

		/* map the dataspace's physical pages to core-local virtual addresses */
		size_t num_pages = page_rounded_size >> get_page_size_log2();
		map_local(ds->phys_addr(), (addr_t)virt_addr, num_pages);

		return virt_addr;
	};

	return _ep.apply(ds_cap, lambda);
}


void Core_region_map::detach(Local_addr core_local_addr)
{
	size_t size = platform_specific()->region_alloc_size_at(core_local_addr);

	if (!unmap_local(core_local_addr, size >> get_page_size_log2())) {
		Genode::error("could not unmap core virtual address ",
		              Hex(core_local_addr), " in ", __PRETTY_FUNCTION__);
		return;
	}

	platform()->region_alloc()->free(core_local_addr);
}
