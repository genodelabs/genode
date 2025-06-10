/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <io_mem_session_component.h>
#include <untyped_memory.h>


using namespace Core;


Io_mem_session_component::Dataspace_attr Io_mem_session_component::_acquire(Phys_range request)
{
	if (!request.req_size)
		return Dataspace_attr();

	auto const size = request.size();
	auto const base = request.base();
	auto const req_base = request.req_base;
	auto const req_size = request.req_size;

	if (size < get_page_size())
		return Dataspace_attr();

	/* check whether first/last pages are already in use by another ds */
	bool first_page_used = _io_mem_alloc.alloc_addr(get_page_size(),
	                                                base).failed();
	bool  last_page_used = _io_mem_alloc.alloc_addr(get_page_size(),
	                                                base + size -
	                                                get_page_size()).failed();

	/* try allocation of whole requested region */
	bool const ok = _io_mem_alloc.alloc_addr(req_size, req_base).convert<bool>(
		[&] (Range_allocator::Allocation &) { return true; },
		[&] (Alloc_error) { return false; });

	if (!ok) {
		error("I/O memory ", Hex_range<addr_t>(req_base, req_size),
		      " not available");
		return Dataspace_attr();
	}

	auto convert_phys = base;
	auto convert_size = size;

	/* if first page was in use beforehand, exclude it from conversion */
	if (first_page_used) {
		convert_phys += get_page_size();
		convert_size -= get_page_size();
	}

	if (convert_size >= get_page_size()) {
		/* if last page was in use beforehand, exclude it from conversion */
		if (last_page_used)
			convert_size -= get_page_size();

		size_t const num_pages = convert_size >> get_page_size_log2();

		if (!Untyped_memory::convert_to_page_frames(convert_phys, num_pages))
			return Dataspace_attr();
	}

	return Dataspace_attr(size, 0 /* no core local mapping */, base,
	                      _cacheable, req_base);
}


void Io_mem_session_component::_release(Dataspace_attr const &attr)
{
	/*
	 * The generic base io_mem implementation already removed the region
	 * from _io_mem_alloc. Now we have to check, which parts must be
	 * converted back to untyped memory. The first and last page of the
	 * region may still be in use by another io-mem allocation, so check
	 * for first/last page and exclude it from conversion if required.
	 */
	auto convert_phys = attr.phys_addr;
	auto convert_size = attr.size;
	auto page_size    = get_page_size();

	if (convert_size >= page_size)
		_io_mem_alloc.alloc_addr(page_size, convert_phys).with_result(
			/* if the region is not occupied anymore, it becomes untyped */
			[&] (Range_allocator::Allocation &) { },
			[&] (Alloc_error) {
				/*
				 * The region is still occupied by another allocation,
				 * exclude the page from conversion back to untyped memory
				 */
				convert_phys += page_size;
				convert_size -= page_size;
			});

	if (convert_size >= page_size)
		_io_mem_alloc.alloc_addr(page_size, convert_phys + convert_size -
			                                page_size).with_result(

			/* if the region is not occupied anymore, it becomes untyped */
			[&] (Range_allocator::Allocation &) { },
			[&] (Alloc_error) {
				/*
				 * The region is still occupied by another allocation,
				 * exclude the page from conversion back to untyped memory
				 */
				convert_size -= page_size;
			});

	auto const num_pages = convert_size >> get_page_size_log2();

	if (num_pages)
		Untyped_memory::convert_to_untyped_frames(convert_phys,
		                                          page_size * num_pages);
}
