/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \date   2010-02-15
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

/* Genode includes */
#include <base/printf.h>
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
	 * \param to_addr    core-local destination address
	 * \param num_pages  number of pages to map
	 *
	 * \return true on success
	 */
	inline bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages)
	{
		return ::map_local((Nova::Utcb *)Thread_base::myself()->utcb(),
		                   from_phys, to_virt, num_pages, true);
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
