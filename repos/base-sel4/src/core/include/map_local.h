/*
 * \brief  Core-local memory mapping
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <util.h>


namespace Genode {

	/**
	 * Map physical pages to core-local virtual address range
	 *
	 * \param from_phys  physical source address
	 * \param to_virt    core-local destination address
	 * \param num_pages  number of pages to map
	 *
	 * \return true on success
	 */
	inline bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages)
	{
		PDBG("not implemented");

		return false;
	}


	inline bool unmap_local(addr_t virt_addr, size_t num_pages)
	{
		PDBG("not implemented");

		return false;
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
