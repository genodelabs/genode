/*
 * \brief  Export and initialize RAM dataspace
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
#include <ram_session_component.h>
#include <platform.h>
#include <map_local.h>

using namespace Genode;


void Ram_session_component::_export_ram_ds(Dataspace_component *ds)
{
	size_t const num_pages = ds->size() >> get_page_size_log2();
	Untyped_memory::convert_to_page_frames(ds->phys_addr(), num_pages);
}


void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds)
{
	PDBG("not implemented");
}


void Ram_session_component::_clear_ds (Dataspace_component *ds)
{
	size_t page_rounded_size = (ds->size() + get_page_size() - 1) & get_page_mask();

	/* allocate range in core's virtual address space */
	void *virt_addr;
	if (!platform()->region_alloc()->alloc(page_rounded_size, &virt_addr)) {
		PERR("could not allocate virtual address range in core of size %zd\n",
		     page_rounded_size);
		return;
	}

	/* map the dataspace's physical pages to core-local virtual addresses */
	size_t num_pages = page_rounded_size >> get_page_size_log2();
	map_local(ds->phys_addr(), (addr_t)virt_addr, num_pages);

	/* clear dataspace */
	size_t num_longwords = page_rounded_size/sizeof(long);
	for (long *dst = (long *)virt_addr; num_longwords--;)
		*dst++ = 0;

	/* unmap dataspace from core */
	unmap_local((addr_t)virt_addr, num_pages);

	/* free core's virtual address space */
	platform()->region_alloc()->free(virt_addr, page_rounded_size);
}
