/*
 * \brief  Export RAM dataspace as shared memory object
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* core includes */
#include <ram_session_component.h>
#include <platform.h>
#include <util.h>
#include <nova_util.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;


void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }


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
		                                              &virt_addr, align_log2).ok())
			break;
	}

	return virt_addr;
}


void Ram_session_component::_clear_ds(Dataspace_component *ds)
{
	size_t page_rounded_size = align_addr(ds->size(), get_page_size_log2());

	size_t memset_count = page_rounded_size / 4;
	addr_t memset_ptr   = ds->core_local_addr();

	if ((memset_count * 4 == page_rounded_size) && !(memset_ptr & 0x3))
		asm volatile ("rep stosl" : "+D" (memset_ptr), "+c" (memset_count)
		                          : "a" (0)  : "memory");
	else
		memset(reinterpret_cast<void *>(memset_ptr), 0, page_rounded_size);

	/* we don't keep any core-local mapping */
	unmap_local(reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb()),
	            ds->core_local_addr(),
	            page_rounded_size >> get_page_size_log2());

	platform()->region_alloc()->free((void*)ds->core_local_addr(),
	                                 page_rounded_size);

	ds->assign_core_local_addr(nullptr);
}


void Ram_session_component::_export_ram_ds(Dataspace_component *ds) {

	size_t page_rounded_size = align_addr(ds->size(), get_page_size_log2());

	/* allocate the virtual region contiguous for the dataspace */
	void * virt_ptr = alloc_region(ds, page_rounded_size);
	if (!virt_ptr)
		throw Out_of_metadata();

	/* map it writeable for _clear_ds */
	Nova::Utcb * const utcb = reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());
	const Nova::Rights rights_rw(true, true, false);

	if (map_local(utcb, ds->phys_addr(), reinterpret_cast<addr_t>(virt_ptr),
	              page_rounded_size >> get_page_size_log2(), rights_rw, true)) {
		platform()->region_alloc()->free(virt_ptr, page_rounded_size);
		throw Out_of_metadata();
	}

	/* assign virtual address to the dataspace to be used by clear_ds */
	ds->assign_core_local_addr(virt_ptr);
}
