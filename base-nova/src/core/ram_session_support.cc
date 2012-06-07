/*
 * \brief  Export RAM dataspace as shared memory object
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
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

void Ram_session_component::_export_ram_ds(Dataspace_component *ds) { }
void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }

void Ram_session_component::_clear_ds(Dataspace_component *ds)
{
	/*
	 * Map dataspace core-locally and clear its content
	 */

	size_t page_rounded_size = (ds->size() + get_page_size() - 1) & get_page_mask();

	/*
	 * Allocate range in core's virtual address space
	 *
	 * Start with trying to use natural alignment. If this does not work,
	 * successively weaken the alignment constraint until we hit the page size.
	 */
	void *virt_addr;
	bool virt_alloc_succeeded = false;
	size_t align_log2 = log2(ds->size());
	for (; align_log2 >= get_page_size_log2(); align_log2--) {
		if (platform()->region_alloc()->alloc_aligned(page_rounded_size,
		                                              &virt_addr, align_log2)) {
			virt_alloc_succeeded = true;
			break;
		}
	}

	if (!virt_alloc_succeeded) {
		PERR("Could not allocate virtual address range in core of size %zd\n",
		     page_rounded_size);
		return;
	}

	if (verbose_ram_ds)
		printf("-- ram ds size=%zx phys %lx has core-local addr %p\n",
		       page_rounded_size, ds->phys_addr(), virt_addr);

	/* map the dataspace's physical pages to local addresses */
	map_local((Nova::Utcb *)Thread_base::myself()->utcb(),
	          ds->phys_addr(), (addr_t)virt_addr,
	          page_rounded_size >> get_page_size_log2(), true);

	memset(virt_addr, 0, page_rounded_size);

	ds->assign_core_local_addr(virt_addr);
}
