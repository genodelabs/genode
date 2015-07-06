/*
 * \brief  Export RAM dataspace as shared memory object
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
#include <base/thread.h>

/* core includes */
#include <ram_session_component.h>
#include <platform.h>
#include <util.h>
#include <nova_util.h>

/* NOVA includes */
#include <nova/syscalls.h>

enum { verbose_ram_ds = false };

using namespace Genode;


void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds)
{
	size_t page_rounded_size = (ds->size() + get_page_size() - 1) & get_page_mask();

	if (verbose_ram_ds)
		printf("-- revoke - ram ds size=0x%8zx phys 0x%8lx has core-local addr 0x%8lx - thread 0x%8p\n",
		       page_rounded_size, ds->phys_addr(), ds->core_local_addr(), Thread_base::myself()->utcb());

	unmap_local((Nova::Utcb *)Thread_base::myself()->utcb(),
	            ds->core_local_addr(),
	            page_rounded_size >> get_page_size_log2());

	platform()->region_alloc()->free((void*)ds->core_local_addr(),
	                                 page_rounded_size);
}


/**
 * Map dataspace core-locally
 */
static inline void * alloc_region(Dataspace_component *ds, const size_t size)
{
	/*
	 * Allocate range in core's virtual address space
	 *
	 * Start with trying to use natural alignment. If this does not work,
	 * successively weaken the alignment constraint until we hit the page size.
	 */
	void *virt_addr = 0;
	size_t align_log2 = log2(ds->size());
	for (; align_log2 >= get_page_size_log2(); align_log2--) {
		if (platform()->region_alloc()->alloc_aligned(size,
		                                              &virt_addr, align_log2).is_ok())
			break;
	}

	return virt_addr;
}


void Ram_session_component::_clear_ds(Dataspace_component *ds)
{
	memset((void *)ds->core_local_addr(), 0, ds->size());
}


void Ram_session_component::_export_ram_ds(Dataspace_component *ds) {

	const size_t page_rounded_size = (ds->size() + get_page_size() - 1) & get_page_mask();

	/* allocate the virtual region contiguous for the dataspace */
	void * virt_ptr = alloc_region(ds, page_rounded_size);
	if (!virt_ptr)
		throw Out_of_metadata();

	/* map it */
	Nova::Utcb * const utcb = reinterpret_cast<Nova::Utcb *>(Thread_base::myself()->utcb());
	const Nova::Rights rights(true, ds->writable(), true);

	if (map_local(utcb, ds->phys_addr(), reinterpret_cast<addr_t>(virt_ptr),
	              page_rounded_size >> get_page_size_log2(), rights, true)) {
		platform()->region_alloc()->free(virt_ptr, page_rounded_size);
		throw Out_of_metadata();
	}

	/* we succeeded, so assign the virtual address to the dataspace */
	ds->assign_core_local_addr(virt_ptr);

	if (verbose_ram_ds)
		printf("-- map    - ram ds size=0x%8zx phys 0x%8lx has core-local addr 0x%8lx\n",
		       page_rounded_size, ds->phys_addr(), ds->core_local_addr());
}
