/*
 * \brief  Client-side region map stub
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <region_map/client.h>

using namespace Genode;


Region_map_client::Region_map_client(Capability<Region_map> session)
: Rpc_client<Region_map>(session) { }


Region_map::Local_addr
Region_map_client::attach(Dataspace_capability ds, size_t size, off_t offset,
                          bool use_local_addr, Local_addr local_addr,
                          bool executable, bool writeable)
{
	return call<Rpc_attach>(ds, size, offset, use_local_addr, local_addr,
	                        executable, writeable);
}


void Region_map_client::detach(Local_addr local_addr) {
	call<Rpc_detach>(local_addr); }


void Region_map_client::fault_handler(Signal_context_capability cap) {
	call<Rpc_fault_handler>(cap); }


Region_map::State Region_map_client::state() { return call<Rpc_state>(); }


Dataspace_capability Region_map_client::dataspace()
{
	if (!_rm_ds_cap.valid())
		_rm_ds_cap = call<Rpc_dataspace>();

	return _rm_ds_cap;
}

