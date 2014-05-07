/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/misc_math.h>

/* core includes */
#include <ram_session_component.h>
#include <platform.h>
#include <map_local.h>


using namespace Genode;

void Ram_session_component::_export_ram_ds(Dataspace_component *ds) { }
void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }

void Ram_session_component::_clear_ds (Dataspace_component *ds)
{
	using namespace Codezero;

	/*
	 * Map dataspace core-locally, memset, unmap dataspace
	 */

	size_t page_rounded_size = (ds->size() + get_page_size() - 1) & get_page_mask();
	size_t num_pages         = page_rounded_size >> get_page_size_log2();

	/* allocate range in core's virtual address space */
	void *virt_addr;
	if (!platform()->region_alloc()->alloc(page_rounded_size, &virt_addr)) {
		PERR("Could not allocate virtual address range in core of size %zd\n",
		     page_rounded_size);
		return;
	}

	/* map the dataspace's physical pages to corresponding virtual addresses */
	if (!map_local(ds->phys_addr(), (addr_t)virt_addr, num_pages)) {
		PERR("core-local memory mapping failed\n");
		return;
	}

	memset(virt_addr, 0, ds->size());

	/* unmap dataspace from core */
	if (!unmap_local((addr_t)virt_addr, num_pages)) {
		PERR("could not unmap %zd pages from virtual address range at %p",
		     num_pages, virt_addr);
		return;
	}

	/* free core's virtual address space */
	platform()->region_alloc()->free(virt_addr, page_rounded_size);
}
