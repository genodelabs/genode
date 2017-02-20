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


void Dataspace_component::attached_to(Rm_region *region)
{
	Lock::Guard lock_guard(_lock);
	_regions.insert(region);
}


void Dataspace_component::detached_from(Rm_region *region)
{
	Lock::Guard lock_guard(_lock);
	_regions.remove(region);
}

void Dataspace_component::detach_from_rm_sessions()
{
	_lock.lock();

	/* remove from all regions */
	while (Rm_region *r = _regions.first()) {

		/*
		 * The 'detach' function calls 'Dataspace_component::detached_from'
		 * and thereby removes the current region from the '_regions' list.
		 */
		_lock.unlock();
		r->rm()->detach((void *)r->base());
		_lock.lock();
	}

	_lock.unlock();
}

Dataspace_component::~Dataspace_component()
{
	detach_from_rm_sessions();
}
