/*
 * \brief  Default implementation of core-local region map
 * \author Norman Feske
 * \date   2009-04-02
 *
 * This implementation assumes that dataspaces are identity-mapped within
 * core. This is the case for traditional L4 kernels.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <core_region_map.h>

using namespace Genode;


Region_map::Local_addr
Core_region_map::attach(Dataspace_capability ds_cap, size_t size,
                        off_t offset, bool use_local_addr,
                        Region_map::Local_addr, bool executable)
{
	auto lambda = [] (Dataspace_component *ds) {
		if (!ds)
			throw Invalid_dataspace();

		return (void *)ds->phys_addr();
	};
	return _ep.apply(ds_cap, lambda);
}


void Core_region_map::detach(Local_addr) { }
