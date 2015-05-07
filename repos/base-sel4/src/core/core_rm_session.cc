/*
 * \brief  Core-local RM session
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <core_rm_session.h>
#include <platform.h>
#include <map_local.h>

using namespace Genode;


Rm_session::Local_addr
Core_rm_session::attach(Dataspace_capability ds_cap, size_t size,
                        off_t offset, bool use_local_addr,
                        Rm_session::Local_addr local_addr,
                        bool executable)
{
	Object_pool<Dataspace_component>::Guard ds(_ds_ep->lookup_and_lock(ds_cap));
	if (!ds)
		throw Invalid_dataspace();

	if (size == 0)
		size = ds->size();

	size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

	if (use_local_addr) {
		PERR("Parameter 'use_local_addr' not supported within core");
		return 0;
	}

	if (offset) {
		PERR("Parameter 'offset' not supported within core");
		return 0;
	}

	/* allocate range in core's virtual address space */
	void *virt_addr;
	if (!platform()->region_alloc()->alloc(page_rounded_size, &virt_addr)) {
		PERR("Could not allocate virtual address range in core of size %zd\n",
		     page_rounded_size);
		return false;
	}

	/* map the dataspace's physical pages to core-local virtual addresses */
	size_t num_pages = page_rounded_size >> get_page_size_log2();
	map_local(ds->phys_addr(), (addr_t)virt_addr, num_pages);

	return virt_addr;
}
