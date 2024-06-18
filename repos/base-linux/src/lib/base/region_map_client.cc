/*
 * \brief  Pseudo region map client stub targeting the process-local implementation
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <region_map/client.h>

/* base-internal includes */
#include <base/internal/local_capability.h>

using namespace Genode;


class Capability_invalid : public Genode::Exception {};


/**
 * Return pointer to locally implemented region map
 *
 * \throw Local_interface::Non_local_capability
 */
static Region_map *_local(Capability<Region_map> cap)
{
	if (!cap.valid())
		throw Capability_invalid();
	return Local_capability<Region_map>::deref(cap);
}


Region_map_client::Region_map_client(Capability<Region_map> session)
: Rpc_client<Region_map>(session) { }


Region_map::Attach_result
Region_map_client::attach(Dataspace_capability ds, Attr const &attr)
{
	return _local(rpc_cap())->attach(ds, attr);
}


void Region_map_client::detach(addr_t at) { return _local(rpc_cap())->detach(at); }


void Region_map_client::fault_handler(Signal_context_capability /*handler*/)
{
	/*
	 * On Linux, page faults are never reflected to the user land. They
	 * are always handled by the kernel. If a segmentation fault
	 * occurs, this condition is being reflected as a CPU exception
	 * to the handler registered via 'Cpu_session::exception_handler'.
	 */
}


Region_map::Fault Region_map_client::fault() { return _local(rpc_cap())->fault(); }


Dataspace_capability Region_map_client::dataspace()
{
	try {
		return _local(rpc_cap())->dataspace();
	} catch (Capability_invalid&) { return Dataspace_capability(); }
}
