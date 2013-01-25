/*
 * \brief  Implementation of the IO_MEM session interface
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-03-29
 *
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <io_mem_session_component.h>
#include <platform.h>
#include <nova_util.h>

using namespace Genode;


void Io_mem_session_component::_unmap_local(addr_t base, size_t size)
{
	size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

	Nova::Rights rwx(true, true, true);
	int count = page_rounded_size >> 12;

	for (int i = 0; i < count; i++)
		unmap_local(Nova::Mem_crd((base >> 12) + i, 0, rwx));
}


addr_t Io_mem_session_component::_map_local(addr_t base, size_t size)
{
	size_t page_rounded_size = (size + get_page_size() - 1) & get_page_mask();

	/* align large I/O dataspaces on a super-page boundary within core */
	size_t alignment = (size >= get_super_page_size()) ? get_super_page_size_log2()
	                                                   : get_page_size_log2();

	/* allocate range in core's virtual address space */
	void *virt_addr;
	if (platform()->region_alloc()->alloc_aligned(page_rounded_size,
	                                               &virt_addr, alignment).is_error()) {
		PERR("Could not allocate virtual address range in core of size %zd\n",
		     page_rounded_size);
		return 0;
	}

	/* map the dataspace's physical pages to local addresses */
	const Nova::Rights rights(true, true, false);
	int res = map_local((Nova::Utcb *)Thread_base::myself()->utcb(),
	                    base, (addr_t)virt_addr,
	                    page_rounded_size >> get_page_size_log2(), rights, true);

	return res ? 0 : (addr_t)virt_addr;
}
