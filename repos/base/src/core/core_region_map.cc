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

using namespace Core;


Region_map::Attach_result
Core_region_map::attach(Dataspace_capability ds_cap, Attr const &)
{
	return _ep.apply(ds_cap, [] (Dataspace_component *ds) -> Attach_result {
		if (!ds)
			return Attach_error::INVALID_DATASPACE;

		return Range { .start = ds->phys_addr(), .num_bytes = ds->size() };
	});
}


void Core_region_map::detach(addr_t) { }
