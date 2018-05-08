/*
 * \brief  Client-side stub for region map
 * \author Norman Feske
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <region_map/client.h>

using namespace Genode;


Region_map_client::Region_map_client(Capability<Region_map> cap)
: Rpc_client<Region_map>(cap) { }


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


Dataspace_capability Region_map_client::dataspace() { return call<Rpc_dataspace>(); }
