/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Norman Feske
 * \date   2006-07-03
 *
 * On L4, each dataspace _is_ a shared memory object.
 * Therefore, these functions are empty.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>

/* core includes */
#include <platform.h>
#include <map_local.h>
#include <ram_dataspace_factory.h>

/* base-internal includes */
#include <base/internal/okl4.h>

using namespace Core;


void Ram_dataspace_factory::_export_ram_ds(Dataspace_component &) { }


void Ram_dataspace_factory::_revoke_ram_ds(Dataspace_component &) { }


void Ram_dataspace_factory::_clear_ds (Dataspace_component &ds)
{
	size_t page_rounded_size = (ds.size() + get_page_size() - 1) & get_page_mask();

	/* allocate range in core's virtual address space */
	platform().region_alloc().try_alloc(page_rounded_size).with_result(
		[&] (Range_allocator::Allocation &virt) {

			/* map the dataspace's physical pages to corresponding virtual addresses */
			size_t num_pages = page_rounded_size >> get_page_size_log2();
			if (!map_local(ds.phys_addr(), (addr_t)virt.ptr, num_pages)) {
				error("core-local memory mapping failed");
				return;
			}

			/* clear dataspace */
			size_t num_longwords = page_rounded_size/sizeof(long);
			for (long *dst = (long *)virt.ptr; num_longwords--;)
				*dst++ = 0;

			/* unmap dataspace from core */
			if (!unmap_local((addr_t)virt.ptr, num_pages))
				error("could not unmap core-local address range at ", virt.ptr, ", "
				      "error=", Okl4::L4_ErrorCode());
		},
		[&] (Alloc_error) {
			error("could not allocate virtual address range of size ", page_rounded_size);
		});
}
