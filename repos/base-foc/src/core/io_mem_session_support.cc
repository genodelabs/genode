/*
 * \brief  Fiasco.OC-specific implementation of the IO_MEM session interface
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-08-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <util.h>
#include <io_mem_session_component.h>
#include <map_local.h>

using namespace Genode;


void Io_mem_session_component::_unmap_local(addr_t base, size_t size)
{
	platform()->region_alloc()->free(reinterpret_cast<void *>(base));
}


addr_t Io_mem_session_component::_map_local(addr_t base, size_t size)
{
	/* align large I/O dataspaces on a super-page boundary within core */
	size_t alignment = (size >= get_super_page_size()) ? get_super_page_size_log2()
	                                                   : get_page_size_log2();

	/* find appropriate region for mapping */
	void *local_base = 0;
	if (platform()->region_alloc()->alloc_aligned(size, &local_base, alignment).error())
		return 0;

	if (!map_local_io(base, (addr_t)local_base, size >> get_page_size_log2())) {
		error("map_local_io failed");
		return 0;
	}

	return (addr_t)local_base;
}
