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

	struct Guard
	{
		Range_allocator &virt_alloc;
		struct { void *virt_ptr = nullptr; };

		Guard(Range_allocator &virt_alloc) : virt_alloc(virt_alloc) { }

		~Guard() { if (virt_ptr) virt_alloc.free(virt_ptr); }

	} guard(platform().region_alloc());

	/* allocate range in core's virtual address space */
	platform().region_alloc().try_alloc(page_rounded_size).with_result(
		[&] (void *ptr) { guard.virt_ptr = ptr; },
		[&] (Range_allocator::Alloc_error e) {
			error("could not allocate virtual address range in core of size ",
			      page_rounded_size, ", error=", e); });

	if (!guard.virt_ptr)
		return;

	/* map the dataspace's physical pages to corresponding virtual addresses */
	size_t num_pages = page_rounded_size >> get_page_size_log2();
	if (!map_local(ds.phys_addr(), (addr_t)guard.virt_ptr, num_pages)) {
		error("core-local memory mapping failed");
		return;
	}

	/* clear dataspace */
	size_t num_longwords = page_rounded_size/sizeof(long);
	for (long *dst = (long *)guard.virt_ptr; num_longwords--;)
		*dst++ = 0;

	/* unmap dataspace from core */
	if (!unmap_local((addr_t)guard.virt_ptr, num_pages))
		error("could not unmap core-local address range at ", guard.virt_ptr, ", "
		      "error=", Okl4::L4_ErrorCode());
}
