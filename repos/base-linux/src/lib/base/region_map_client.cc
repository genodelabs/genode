/*
 * \brief  Pseudo region map client stub targeting the process-local implementation
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <region_map/client.h>

/* base-internal includes */
#include <base/internal/local_capability.h>

using namespace Genode;


/**
 * Return pointer to locally implemented region map
 *
 * \throw Local_interface::Non_local_capability
 */
static Region_map *_local(Capability<Region_map> cap)
{
	return Local_capability<Region_map>::deref(cap);
}


Region_map_client::Region_map_client(Capability<Region_map> session)
: Rpc_client<Region_map>(session) { }


Region_map::Local_addr
Region_map_client::attach(Dataspace_capability ds, size_t size,
                          off_t offset, bool use_local_addr,
                          Region_map::Local_addr local_addr,
                          bool executable)
{
	return _local(*this)->attach(ds, size, offset, use_local_addr,
	                             local_addr, executable);
}


void Region_map_client::detach(Local_addr local_addr) {
	return _local(*this)->detach(local_addr); }


void Region_map_client::fault_handler(Signal_context_capability /*handler*/)
{
	/*
	 * On Linux, page faults are never reflected to the user land. They
	 * are always handled by the kernel. If a segmentation fault
	 * occurs, this condition is being reflected as a CPU exception
	 * to the handler registered via 'Cpu_session::exception_handler'.
	 */
}


Region_map::State Region_map_client::state() { return _local(*this)->state(); }


Dataspace_capability Region_map_client::dataspace() {
	return _local(*this) ? _local(*this)->dataspace() : Dataspace_capability(); }

