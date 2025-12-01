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
#include <core_local_rm.h>
#include <platform.h>
#include <util.h>
#include <nova_util.h>
#include <dataspace_component.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Core;


/**
 * Map dataspace core-locally
 */
static inline void * alloc_region(Dataspace_component &ds, const size_t size)
{
	using Region_allocation = Range_allocator::Allocation;

	if (size == 0)
		return nullptr;

	/*
	 * Allocate range in core's virtual address space
	 *
	 * Start with trying to use natural alignment. If this does not work,
	 * successively weaken the alignment constraint until we hit the page size.
	 */
	void *virt_addr = 0;
	Align align { .log2 = log2(ds.size(), Genode::PAGE_SIZE_LOG2) };
	for (; align.log2 >= AT_PAGE.log2; align.log2--) {

		platform().region_alloc().alloc_aligned(size, align).with_result(
			[&] (Region_allocation &a) { a.deallocate = false; virt_addr = a.ptr; },
			[&] (Alloc_error) { });

		if (virt_addr)
			break;
	}

	return virt_addr;
}

Core_local_rm::Result
Core_local_rm::attach(Dataspace_capability ds_cap, Attach_attr const &attr)
{
	return _ep.apply(ds_cap, [&] (Dataspace_component * const ds_ptr) -> Result {

		if (!ds_ptr)
			return Error::INVALID_DATASPACE;

		Dataspace_component &ds = *ds_ptr;

		/* attach attributes 'use_at' and 'offset' not supported within core */
		if (attr.use_at || attr.offset)
			return Error::REGION_CONFLICT;

		const size_t page_rounded_size = align_addr(ds.size(), AT_PAGE);

		/* allocate the virtual region contiguous for the dataspace */
		void * virt_ptr = alloc_region(ds, page_rounded_size);
		if (!virt_ptr)
			return Error::OUT_OF_RAM;

		/* map it */
		Nova::Utcb &utcb = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());
		const Nova::Rights rights(true, attr.writeable && ds.writeable(), attr.executable);

		if (map_local(platform_specific().core_pd_sel(), utcb,
		              ds.phys_addr(), reinterpret_cast<addr_t>(virt_ptr),
		              page_rounded_size >> PAGE_SIZE_LOG2, rights, true)) {
			platform().region_alloc().free(virt_ptr, page_rounded_size);

			return Error::OUT_OF_RAM;
		}
		return { *this, { virt_ptr, page_rounded_size } };
	});
}


void Core_local_rm::_free(Attachment &a)
{
	size_t size = platform_specific().region_alloc_size_at(a.ptr);

	unmap_local(*reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb()),
	            addr_t(a.ptr), size >> PAGE_SIZE_LOG2);

	platform().region_alloc().free(a.ptr);
}
