/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Norman Feske
 * \date   2006-07-03
 *
 * On L4, each dataspace _is_ a shared memory object.
 * Therefore, these functions are empty.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>

/* core includes */
#include <platform.h>
#include <map_local.h>
#include <ram_session_component.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/ipc.h>  /* needed for 'L4_ErrorCode' */
} }

using namespace Genode;

void Ram_session_component::_export_ram_ds(Dataspace_component *ds) { }
void Ram_session_component::_revoke_ram_ds(Dataspace_component *ds) { }

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

	/* map the dataspace's physical pages to corresponding virtual addresses */
	size_t num_pages = page_rounded_size >> get_page_size_log2();
	if (!map_local(ds->phys_addr(), (addr_t)virt_addr, num_pages)) {
		PERR("core-local memory mapping failed, Error Code=%d\n", (int)Okl4::L4_ErrorCode());
		return;
	}

	/* clear dataspace */
	size_t num_longwords = page_rounded_size/sizeof(long);
	for (long *dst = (long *)virt_addr; num_longwords--;)
		*dst++ = 0;

	/* unmap dataspace from core */
	if (!unmap_local((addr_t)virt_addr, num_pages))
		PERR("could not unmap core-local address range at %p (Error Code %ld)",
		     virt_addr, Okl4::L4_ErrorCode());

	/* free core's virtual address space */
	platform()->region_alloc()->free(virt_addr, page_rounded_size);
}
