/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \date   2010-02-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
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
	 * On Codezero, mappings originate from the physical address space.
	 *
	 * \param from_phys  physical source address
	 * \param to_virt    core-local destination address
	 * \param num_pages  number of pages to map
	 *
	 * \return true on success
	 */
	inline bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages)
	{
		using namespace Codezero;

		int res = l4_map((void *)from_phys, (void *)to_virt,
		                 num_pages, MAP_USR_RW, thread_myself());
		if (res < 0) {
			PERR("l4_map phys 0x%lx -> 0x%lx returned %d", from_phys, to_virt, res);
			return false;
		}

		return true;
	}


	inline bool unmap_local(addr_t virt_addr, size_t num_pages)
	{
		using namespace Codezero;

		int res = l4_unmap((void *)virt_addr, num_pages, thread_myself());
		if (res < 0) {
			PERR("l4_unmap returned %d", res);
			return false;
		}

		return true;
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
