/*
 * \brief  Core-local mapping
 * \author Stefan Kalkowski
 * \date   2014-02-26
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

/* core includes */
#include <page_flags.h>

namespace Genode {

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
	               Page_flags flags = { true, true, false, false,
	                                    false, CACHED });

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

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
