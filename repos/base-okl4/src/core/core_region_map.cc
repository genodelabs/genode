/*
 * \brief  OKL4-specific implementation of core-local region map
 * \author Norman Feske
 * \date   2009-04-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <core_region_map.h>
#include <map_local.h>

using namespace Genode;


Region_map::Local_addr
Core_region_map::attach(Dataspace_capability ds_cap, size_t size,
                        off_t offset, bool use_local_addr,
                        Region_map::Local_addr, bool, bool)
{
	using namespace Okl4;

	auto lambda = [&] (Dataspace_component *ds) -> void *
	{
		if (!ds)
			throw Invalid_dataspace();

		if (size == 0)
			size = ds->size();

		size_t const page_rounded_size = (size + get_page_size() - 1)
		                               & get_page_mask();

		if (use_local_addr) {
			error("parameter 'use_local_addr' not supported within core");
			return nullptr;
		}

		if (offset) {
			error("parameter 'offset' not supported within core");
			return nullptr;
		}

		/* allocate range in core's virtual address space */
		void *virt_addr;
		if (!platform().region_alloc().alloc(page_rounded_size, &virt_addr)) {
			error("could not allocate virtual address range in core of size ",
			      page_rounded_size);
			return nullptr;
		}

		/* map the dataspace's physical pages to corresponding virtual addresses */
		unsigned num_pages = page_rounded_size >> get_page_size_log2();
		if (!map_local(ds->phys_addr(), (addr_t)virt_addr, num_pages))
			return nullptr;
		return virt_addr;
	};

	return _ep.apply(ds_cap, lambda);
}


void Core_region_map::detach(Local_addr) { }
