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


Region_map::Attach_result
Region_map_client::attach(Dataspace_capability ds, Attr const &attr)
{
	return call<Rpc_attach>(ds, attr);
}


void Region_map_client::detach(addr_t at) {
	call<Rpc_detach>(at); }


void Region_map_client::fault_handler(Signal_context_capability cap) {
	call<Rpc_fault_handler>(cap); }


Region_map::Fault Region_map_client::fault() { return call<Rpc_fault>(); }


Dataspace_capability Region_map_client::dataspace()
{
	if (!_rm_ds_cap.valid())
		_rm_ds_cap = call<Rpc_dataspace>();

	return _rm_ds_cap;
}

