/*
 * \brief   Dataspace component
 * \date    2006-09-18
 * \author  Christian Helmuth
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <dataspace_component.h>
#include <region_map_component.h>

using namespace Genode;


void Dataspace_component::attached_to(Rm_region &region)
{
	Mutex::Guard lock_guard(_mutex);
	_regions.insert(&region);
}


void Dataspace_component::detached_from(Rm_region &region)
{
	Mutex::Guard lock_guard(_mutex);
	_regions.remove(&region);
}

void Dataspace_component::detach_from_rm_sessions()
{
	_mutex.acquire();

	/* remove from all regions */
	while (Rm_region *r = _regions.first()) {

		/*
		 * The 'detach' function calls 'Dataspace_component::detached_from'
		 * and thereby removes the current region from the '_regions' list.
		 */
		_mutex.release();
		r->rm().detach((void *)r->base());
		_mutex.acquire();
	}

	_mutex.release();
}

Dataspace_component::~Dataspace_component()
{
	detach_from_rm_sessions();
}
