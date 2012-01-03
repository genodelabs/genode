/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \date   2010-02-15
 *
 * These functions are normally doing nothing because core's using 1-to-1 paging at kernel
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__INCLUDE__MAP_LOCAL_H_
#define _SRC__CORE__INCLUDE__MAP_LOCAL_H_

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <util.h>


namespace Genode {

	/**
	 * Map physical pages to core-local virtual address range
	 *
	 * Always true because roottask pager handles all core page faults
	 */
	inline bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages)
	{
		return true;
	}

	/**
	 * Unmap virtual pages from core-local virtual address range
	 *
	 * Does nothing because roottask pager handles all core page faults
	 */
	inline bool unmap_local(addr_t virt_addr, size_t num_pages)
	{
		return true;
	}
}

#endif /* _SRC__CORE__INCLUDE__MAP_LOCAL_H_ */
