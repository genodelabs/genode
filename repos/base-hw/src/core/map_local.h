/*
 * \brief  Core-local mapping
 * \author Stefan Kalkowski
 * \date   2014-02-26
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__MAP_LOCAL_H_
#define _CORE__MAP_LOCAL_H_

/* core includes */
#include <types.h>
#include <cpu/page_flags.h>

namespace Core {

	using Genode::Page_flags;

	/**
	 * Map physical pages to core-local virtual address range
	 *
	 * \param from_phys  physical source address
	 * \param to_virt    core-local destination address
	 * \param num_pages  number of pages to map
	 * \param io_mem     true if it's memory mapped I/O (uncached)
	 *
	 * \return true on success
	 */
	bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages,
	               Page_flags flags = Genode::PAGE_FLAGS_KERN_DATA);

	/**
	 * Unmap pages from core's address space
	 *
	 * \param virt_addr  first core-local address to unmap, must be page-aligned
	 * \param num_pages  number of pages to unmap
	 *
	 * \return true on success
	 */
	bool unmap_local(addr_t virt_addr, size_t num_pages);
}

#endif /* _CORE__MAP_LOCAL_H_ */
