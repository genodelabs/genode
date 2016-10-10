/*
 * \brief  Testing nested region maps
 * \author Norman Feske
 * \date   2008-09-27
 *
 * The program uses two threads. A local fault-handler thread waits for fault
 * signals regarding a sub-region maps that is mapped into the local
 * address space as a dataspace. If a fault occurs, this thread allocates a new
 * dataspace and attaches it to the faulting address to resolve the fault. The
 * main thread performs memory accesses at the local address range that is
 * backed by the region map. Thereby, it triggers region-map faults.
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/thread.h>
#include <base/signal.h>
#include <cap_session/connection.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <dataspace/client.h>

using namespace Genode;


enum {
	MANAGED_SIZE = 0x00010000,
	PAGE_SIZE    = 4096,
};


/**
 * Region-manager fault handler resolves faults by attaching new dataspaces
 */
class Local_fault_handler : public Thread_deprecated<4096>
{
	private:

		Region_map      &_region_map;
		Signal_receiver &_receiver;

	public:

		Local_fault_handler(Region_map &region_map, Signal_receiver &receiver)
		:
			Thread_deprecated("local_fault_handler"),
			_region_map(region_map), _receiver(receiver)
		{ }

		void handle_fault()
		{
			Region_map::State state = _region_map.state();

			log("region-map state is ",
			       state.type == Region_map::State::READ_FAULT  ? "READ_FAULT"  :
			       state.type == Region_map::State::WRITE_FAULT ? "WRITE_FAULT" :
			       state.type == Region_map::State::EXEC_FAULT  ? "EXEC_FAULT"  : "READY",
			       ", pf_addr=", Hex(state.addr, Hex::PREFIX));

			log("allocate dataspace and attach it to sub region map");
			Dataspace_capability ds = env()->ram_session()->alloc(PAGE_SIZE);
			_region_map.attach_at(ds, state.addr & ~(PAGE_SIZE - 1));

			log("returning from handle_fault");
		}

		void entry()
		{
			while (true) {
				log("fault handler: waiting for fault signal");
				Signal signal = _receiver.wait_for_signal();
				log("received ", signal.num(), " fault signals");
				for (unsigned i = 0; i < signal.num(); i++)
					handle_fault();
			}
		}
};


int main(int argc, char **argv)
{
	log("--- nested region map test ---");

	/*
	 * Initialize sub region map and set up a local fault handler for it.
	 */
	static Rm_connection rm;
	static Region_map_client region_map(rm.create(MANAGED_SIZE));
	static Cap_connection cap;
	static Signal_receiver receiver;
	static Signal_context context;
	region_map.fault_handler(receiver.manage(&context));
	static Local_fault_handler fault_handler(region_map, receiver);
	fault_handler.start();

	/*
	 * Attach region map as dataspace to the local address space.
	 */
	void *addr = env()->rm_session()->attach(region_map.dataspace());

	log("attached sub dataspace at local address ", addr);
	Dataspace_client client(region_map.dataspace());
	log("sub dataspace size is ", client.size(), " should be ",
	    (size_t)MANAGED_SIZE);

	/*
	 * Walk through the address range belonging to the region map
	 */
	char *managed = (char *)addr;
	for (int i = 0; i < MANAGED_SIZE; i += PAGE_SIZE/16) {
		log("write to ", (void*)&managed[i]);
		managed[i] = 13;
	}

	receiver.dissolve(&context);

	log("test destruction of region_map");
	Capability<Region_map> rcap = rm.create(4096);
	rm.destroy(rcap);

	log("--- finished nested region map test ---");
	return 0;
}
