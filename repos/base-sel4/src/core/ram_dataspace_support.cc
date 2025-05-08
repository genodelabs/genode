/*
 * \brief  Export and initialize RAM dataspace
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <ram_dataspace_factory.h>
#include <platform.h>
#include <map_local.h>
#include <untyped_memory.h>

using namespace Core;


bool Ram_dataspace_factory::_export_ram_ds(Dataspace_component &ds)
{
	size_t const page_rounded_size = (ds.size() + get_page_size() - 1) & get_page_mask();
	size_t const num_pages = page_rounded_size >> get_page_size_log2();

	Untyped_memory::convert_to_page_frames(ds.phys_addr(), num_pages);
	return true;
}


void Ram_dataspace_factory::_revoke_ram_ds(Dataspace_component &ds)
{
	size_t const page_rounded_size = (ds.size() + get_page_size() - 1) & get_page_mask();

	Untyped_memory::convert_to_untyped_frames(ds.phys_addr(), page_rounded_size);
}


void Ram_dataspace_factory::_clear_ds (Dataspace_component &ds)
{
	size_t const page_rounded_size = (ds.size() + get_page_size() - 1) & get_page_mask();

	/* allocate one page in core's virtual address space */
	auto alloc_one_virt_page = [&] () -> void *
	{
		return platform().region_alloc().try_alloc(get_page_size()).convert<void *>(
			[&] (Range_allocator::Allocation &a) {
				a.deallocate = false; return a.ptr; },
			[&] (Alloc_error) -> void * {
				ASSERT_NEVER_CALLED; });
	};

	addr_t const virt_addr = (addr_t)alloc_one_virt_page();

	/* map each page of dataspace one at a time and clear it */
	for (addr_t offset = 0; offset < page_rounded_size; offset += get_page_size())
	{
		addr_t const phys_addr = ds.phys_addr() + offset;
		enum { ONE_PAGE = 1 };

		/* map one physical page to the core-local address */
		if (!map_local(phys_addr, virt_addr, ONE_PAGE)) {
			ASSERT(!"could not map 4k inside core");
		}

		/* clear one page */
		size_t num_longwords = get_page_size()/sizeof(long);
		for (long *dst = reinterpret_cast<long *>(virt_addr); num_longwords--;)
			*dst++ = 0;

		/* unmap cleared page from core */
		unmap_local(virt_addr, ONE_PAGE, nullptr, ds.cacheability() != CACHED);
	}

	/* free core's virtual address space */
	platform().region_alloc().free((void *)virt_addr, get_page_size());
}
