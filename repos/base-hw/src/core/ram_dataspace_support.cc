/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/error.h>

/* core includes */
#include <ram_dataspace_factory.h>
#include <platform.h>
#include <map_local.h>
#include <util/misc_math.h>

using namespace Core;


void Ram_dataspace_factory::_export_ram_ds(Dataspace_component &) { }


void Ram_dataspace_factory::_revoke_ram_ds(Dataspace_component &) { }


void Ram_dataspace_factory::_clear_ds (Dataspace_component &ds)
{
	using Virt_allocation = Range_allocator::Allocation;

	size_t page_rounded_size = (ds.size() + get_page_size() - 1) & get_page_mask();

	/* allocate range in core's virtual address space */
	Range_allocator::Result const virt =
		platform().region_alloc().try_alloc(page_rounded_size);

	if (virt.failed()) {
		error("clear_ds could not allocate virtual address range of size ",
		      page_rounded_size);
		return;
	}

	addr_t const virt_addr = virt.convert<addr_t>(
		[&] (Virt_allocation const &a) { return addr_t(a.ptr); },
		[]  (Alloc_error)              { return 0UL; });

	/*
	 * Map and clear dataspaces in chucks of 128MiB max to prevent large
	 * mappings from filling up core's page table.
	 */
	static size_t constexpr max_chunk_size = 128 * 1024 * 1024;

	size_t size_remaining = page_rounded_size;
	addr_t chunk_phys_addr = ds.phys_addr();

	while (size_remaining) {
		size_t const chunk_size = min(size_remaining, max_chunk_size);
		size_t const num_pages = chunk_size >> get_page_size_log2();

		/* map the dataspace's physical pages to corresponding virtual addresses */
		if (!map_local(chunk_phys_addr, virt_addr, num_pages)) {
			error("core-local memory mapping failed");
			return;
		}

		/* dependent on the architecture, cache maintainance might be necessary */
		Cpu::clear_memory_region(virt_addr, chunk_size,
		                         ds.cacheability() != CACHED);

		/* unmap dataspace from core */
		if (!unmap_local(virt_addr, num_pages))
			error("could not unmap core-local address range at ", Hex(virt_addr));

		size_remaining  -= chunk_size;
		chunk_phys_addr += chunk_size;
	}
}

