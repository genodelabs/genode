/*
 * \brief  Core-local region map
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <core_region_map.h>
#include <platform.h>
#include <util.h>
#include <nova_util.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Core;


/**
 * Map dataspace core-locally
 */
static inline void * alloc_region(Dataspace_component &ds, const size_t size)
{
	/*
	 * Allocate range in core's virtual address space
	 *
	 * Start with trying to use natural alignment. If this does not work,
	 * successively weaken the alignment constraint until we hit the page size.
	 */
	void *virt_addr = 0;
	size_t align_log2 = log2(ds.size());
	for (; align_log2 >= get_page_size_log2(); align_log2--) {

		platform().region_alloc().alloc_aligned(size, (unsigned)align_log2).with_result(
			[&] (void *ptr) { virt_addr = ptr; },
			[&] (Allocator::Alloc_error) { });

		if (virt_addr)
			break;
	}

	return virt_addr;
}

Region_map::Attach_result
Core_region_map::attach(Dataspace_capability ds_cap, Attr const &attr)
{
	return _ep.apply(ds_cap, [&] (Dataspace_component * const ds_ptr) -> Attach_result {

		if (!ds_ptr)
			return Attach_error::INVALID_DATASPACE;

		Dataspace_component &ds = *ds_ptr;

		/* attach attributes 'use_at' and 'offset' not supported within core */
		if (attr.use_at || attr.offset)
			return Attach_error::REGION_CONFLICT;

		const size_t page_rounded_size = align_addr(ds.size(), get_page_size_log2());

		/* allocate the virtual region contiguous for the dataspace */
		void * virt_ptr = alloc_region(ds, page_rounded_size);
		if (!virt_ptr)
			return Attach_error::OUT_OF_RAM;

		/* map it */
		Nova::Utcb &utcb = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());
		const Nova::Rights rights(true, attr.writeable && ds.writeable(), attr.executable);

		if (map_local(platform_specific().core_pd_sel(), utcb,
		              ds.phys_addr(), reinterpret_cast<addr_t>(virt_ptr),
		              page_rounded_size >> get_page_size_log2(), rights, true)) {
			platform().region_alloc().free(virt_ptr, page_rounded_size);

			return Attach_error::OUT_OF_RAM;
		}

		return Range { .start = addr_t(virt_ptr), .num_bytes = page_rounded_size };
	});
}


void Core_region_map::detach(addr_t core_local_addr)
{
	size_t size = platform_specific().region_alloc_size_at((void *)core_local_addr);

	unmap_local(*reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb()),
	            core_local_addr, size >> get_page_size_log2());

	platform().region_alloc().free((void *)core_local_addr);
}
