/*
 * \brief  Core-local memory mapping
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

/* core includes */
#include <util.h>
#include <platform.h>


namespace Genode {

	/**
	 * Map physical pages to core-local virtual address range
	 *
	 * \param from_phys  physical source address
	 * \param to_virt    core-local destination address
	 * \param num_pages  number of pages to map
	 * \param platform   pointer to platform object (to avoid deadlocks during
	 *                   early Platform() construction caused by nested calls
	 *                   of platform_specific())
	 *
	 * \return true on success
	 */
	inline bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages,
	                      Platform * platform = nullptr)
	{
		enum { DONT_FLUSH = false, WRITEABLE = true, NON_EXECUTABLE = false };
		try {
			platform = platform ? platform : &platform_specific();
			platform->core_vm_space().map(from_phys, to_virt, num_pages,
			                              Cache_attribute::CACHED,
			                              WRITEABLE, NON_EXECUTABLE,
			                              DONT_FLUSH);
		} catch (Page_table_registry::Mapping_cache_full) {
			return false;
		}
		return true;
	}


	/**
	 * Flush memory mappings from core-local virtual address range
	 */
	inline bool unmap_local(addr_t const virt_addr, size_t const num_pages,
	                        Platform * platform = nullptr,
	                        bool const invalidate = false)
	{
		platform = platform ? platform : &platform_specific();
		return platform->core_vm_space().unmap(virt_addr, num_pages, invalidate);
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
