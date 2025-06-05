/*
 * \brief  Fiasco.OC-specific implementation of the IO_MEM session interface
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-08-28
 */

/*
 * Copyright (C) 2006-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <util.h>
#include <io_mem_session_component.h>
#include <map_local.h>

using namespace Core;


Io_mem_session_component::Dataspace_attr Io_mem_session_component::_acquire(Phys_range request)
{
	if (!request.req_size)
		return Dataspace_attr();

	auto const size = request.size();
	auto const base = request.base();

	/* align large I/O dataspaces on a super-page boundary within core */
	auto const alignment = unsigned(size >= get_super_page_size()
	                     ? get_super_page_size_log2()
	                     : get_page_size_log2());

	/* find appropriate region and map it locally */
	return platform().region_alloc().alloc_aligned(size, alignment).convert<Dataspace_attr>(
		[&] (Range_allocator::Allocation &local) {
			if (!map_local_io(base, addr_t(local.ptr), size >> get_page_size_log2())) {
				error("map_local_io failed ", Hex_range(base, size));
				return Dataspace_attr();
			}
			local.deallocate = false;

			return Dataspace_attr(size, addr_t(local.ptr),
			                      base, _cacheable, request.req_base);
		}, [&] (Alloc_error) {
			error("allocation of virtual memory for local I/O mapping failed");
			return Dataspace_attr();
	});
}


void Io_mem_session_component::_release(Dataspace_attr const &attr)
{
	auto const base = attr.core_local_addr;

	if (!base)
		return;

	unmap_local(base, attr.size >> 12);
	platform().region_alloc().free(reinterpret_cast<void *>(base));
}
