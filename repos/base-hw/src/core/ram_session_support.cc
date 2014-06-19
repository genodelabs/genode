/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-02-12
 *
 * TODO: this file is almost identical to
 *       base-okl4/src/core/ram_session_support.cc, we should merge them
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <ram_session_component.h>
#include <platform.h>
#include <map_local.h>

using namespace Genode;

void Ram_session_component::_export_ram_ds(Dataspace_component *ds) { }
void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }

void Ram_session_component::_clear_ds (Dataspace_component * ds)
{
	size_t page_rounded_size = (ds->size() + get_page_size() - 1) & get_page_mask();

	/* allocate range in core's virtual address space */
	void *virt_addr;
	if (!platform()->region_alloc()->alloc(page_rounded_size, &virt_addr)) {
		PERR("could not allocate virtual address range in core of size %zd\n",
		     page_rounded_size);
		return;
	}

	/* map the dataspace's physical pages to corresponding virtual addresses */
	size_t num_pages = page_rounded_size >> get_page_size_log2();
	if (!map_local(ds->phys_addr(), (addr_t)virt_addr, num_pages)) {
		PERR("core-local memory mapping failed");
		return;
	}

	/* clear dataspace */
	memset(virt_addr, 0, page_rounded_size);

	/* uncached dataspaces need to be flushed */
	if (ds->cacheability() != CACHED)
		Kernel::update_data_region((addr_t)virt_addr, page_rounded_size);

	/* unmap dataspace from core */
	if (!unmap_local((addr_t)virt_addr, num_pages))
		PERR("could not unmap core-local address range at %p", virt_addr);

	/* free core's virtual address space */
	platform()->region_alloc()->free(virt_addr, page_rounded_size);
}

