/*
 * \brief  Implementation of the RM session interface
 * \author Christian Prochaska
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <dataspace/client.h>

/* local includes */
#include "rm_session_component.h"

static bool const verbose = false;


/**************************************
 ** Region-manager-session component **
 **************************************/

using namespace Gdb_monitor;

Rm_session_component::Region *Rm_session_component::find_region(void *local_addr, addr_t *offset_in_region)
{
	Lock::Guard lock_guard(_region_map_lock);

//	PDBG("local_addr = %p", local_addr);

	Region *first = _region_map.first();
	Region *region = first ? first->find_by_addr(local_addr) : 0;

	if (!region)
		return 0;

	*offset_in_region = ((addr_t)local_addr - (addr_t)region->start());
//	PDBG("offset_in_region = %lx", *offset_in_region);

	Dataspace_object *managed_ds_obj = _managed_ds_map->obj_by_cap(region->ds_cap());
	if (managed_ds_obj) {
//		PDBG("managed dataspace detected");
		region = managed_ds_obj->rm_session_component()->find_region((void*)*offset_in_region, offset_in_region);
//		if (region)
//			PDBG("found sub region: start = %p, offset = %lx", region->start(), *offset_in_region);
	}

	return region;
}


Rm_session::Local_addr
Rm_session_component::attach(Dataspace_capability ds_cap, size_t size,
                             off_t offset, bool use_local_addr,
                             Rm_session::Local_addr local_addr,
                             bool executable)
{
	if (verbose)
		PDBG("size = %zd, offset = %x", size, (unsigned int)offset);

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

	void *addr = _parent_rm_session.attach(ds_cap, size, offset,
	                                       use_local_addr, local_addr,
	                                       executable);

	Lock::Guard lock_guard(_region_map_lock);
	_region_map.insert(new (env()->heap()) Region(addr, (void*)((addr_t)addr + size - 1), ds_cap, offset));

	if (verbose)
		PDBG("region: %p - %p", addr, (void*)((addr_t)addr + size - 1));

	return addr;
}


void Rm_session_component::detach(Rm_session::Local_addr local_addr)
{
	if (verbose)
		PDBG("local_addr = %p", (void *)local_addr);

	_parent_rm_session.detach(local_addr);

	Lock::Guard lock_guard(_region_map_lock);
	Region *region = _region_map.first()->find_by_addr(local_addr);
	if (!region) {
		PERR("address not in region map");
		return;
	}
	_region_map.remove(region);
	destroy(env()->heap(), region);
}


Pager_capability Rm_session_component::add_client(Thread_capability thread)
{
	if (verbose)
		PDBG("add_client()");

	return _parent_rm_session.add_client(thread);
}


void Rm_session_component::fault_handler(Signal_context_capability handler)
{
	if (verbose)
		PDBG("fault_handler()");

	_parent_rm_session.fault_handler(handler);
}


Rm_session::State Rm_session_component::state()
{
	if (verbose)
		PDBG("state()");

	return _parent_rm_session.state();
}


Dataspace_capability Rm_session_component::dataspace()
{
	if (verbose)
		PDBG("dataspace()");

	Dataspace_capability ds_cap = _parent_rm_session.dataspace();
	_managed_ds_map->insert(new (env()->heap()) Dataspace_object(ds_cap, this));
	return ds_cap;
}

Rm_session_component::Rm_session_component
(Object_pool<Dataspace_object> *managed_ds_map, const char *args)
: _parent_rm_session(env()->parent()->session<Rm_session>(args)),
  _managed_ds_map(managed_ds_map)
{
	if (verbose)
		PDBG("Rm_session_component()");
}


Rm_session_component::~Rm_session_component()
{ }
