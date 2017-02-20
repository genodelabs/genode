/*
 * \brief  Implementation of the region map interface
 * \author Christian Prochaska
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <dataspace/client.h>
#include <util/retry.h>

/* local includes */
#include "region_map_component.h"


/**************************************
 ** Region map component **
 **************************************/

using namespace Gdb_monitor;

Region_map_component::Region *Region_map_component::find_region(void *local_addr, addr_t *offset_in_region)
{
	Lock::Guard lock_guard(_region_map_lock);

	Region *first = _region_map.first();
	Region *region = first ? first->find_by_addr(local_addr) : 0;

	if (!region)
		return 0;

	*offset_in_region = ((addr_t)local_addr - (addr_t)region->start());

	_managed_ds_map.apply(region->ds_cap(), [&] (Dataspace_object *managed_ds_obj) {
		if (managed_ds_obj)
			region =
				managed_ds_obj->region_map_component()->find_region((void*)*offset_in_region,
				                                                    offset_in_region);
	});

	return region;
}


Region_map::Local_addr
Region_map_component::attach(Dataspace_capability ds_cap, size_t size,
                             off_t offset, bool use_local_addr,
                             Region_map::Local_addr local_addr,
                             bool executable)
{
	size_t ds_size = Dataspace_client(ds_cap).size();

	if (offset < 0 || (size_t)offset >= ds_size) {
		PWRN("offset outside of dataspace");
		throw Invalid_args();
	}

	if (size == 0)
		size = ds_size - offset;
	else if (size > ds_size - offset) {
		PWRN("size bigger than remainder of dataspace");
		throw Invalid_args();
	}

	void *addr = _parent_region_map.attach(ds_cap, size, offset,
	                                       use_local_addr, local_addr,
	                                       executable);

	Lock::Guard lock_guard(_region_map_lock);
	_region_map.insert(new (_alloc) Region(addr, (void*)((addr_t)addr + size - 1), ds_cap, offset));

	return addr;
}


void Region_map_component::detach(Region_map::Local_addr local_addr)
{
	_parent_region_map.detach(local_addr);

	Lock::Guard lock_guard(_region_map_lock);
	Region *region = _region_map.first()->find_by_addr(local_addr);
	if (!region) {
		PERR("address not in region map");
		return;
	}
	_region_map.remove(region);
	destroy(_alloc, region);
}


void Region_map_component::fault_handler(Signal_context_capability handler)
{
	_parent_region_map.fault_handler(handler);
}


Region_map::State Region_map_component::state()
{
	return _parent_region_map.state();
}


Dataspace_capability Region_map_component::dataspace()
{
	Dataspace_capability ds_cap = _parent_region_map.dataspace();
	_managed_ds_map.insert(new (_alloc) Dataspace_object(ds_cap, this));
	return ds_cap;
}

Region_map_component::Region_map_component(Rpc_entrypoint &ep,
                                           Allocator &alloc,
                                           Dataspace_pool &managed_ds_map,
                                           Pd_session_capability pd,
                                           Capability<Region_map> parent_region_map)
:
	_ep(ep),
	_alloc(alloc),
	_pd(pd),
	_parent_region_map(parent_region_map),
	_managed_ds_map(managed_ds_map)
{
	_ep.manage(this);
}


Region_map_component::~Region_map_component()
{
	_ep.dissolve(this);
}
