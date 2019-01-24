/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \date   2010-02-15
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

/* Genode includes */
#include <base/thread.h>

/* core includes */
#include <nova_util.h>

namespace Genode {

	/**
	 * Map pages locally within core
	 *
	 * On NOVA, address-space mappings from core to core originate always from
	 * the physical address space.
	 *
	 * \param from_phys  physical source address
	 * \param to_virt    core-local destination address
	 * \param num_pages  number of pages to map
	 *
	 * \return true on success
	 */
	inline bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages,
	                      bool read = true, bool write = true, bool exec = true)
	{
		return (::map_local(platform_specific().core_pd_sel(),
		                    *(Nova::Utcb *)Thread::myself()->utcb(),
		                    from_phys, to_virt, num_pages,
		                    Nova::Rights(read, write, exec), true) == 0);
	}

	/**
	 * Unmap pages locally within core
	 *
	 * \param virt       core-local address
	 * \param num_pages  number of pages to unmap
	 */
	inline void unmap_local(addr_t virt, size_t num_pages)
	{
		::unmap_local(*(Nova::Utcb *)Thread::myself()->utcb(), virt, num_pages);
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
