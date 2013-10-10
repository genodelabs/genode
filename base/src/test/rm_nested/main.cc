/*
 * \brief  Testing nested region-manager sessions
 * \author Norman Feske
 * \date   2008-09-27
 *
 * The program uses two threads. A local fault-handler thread waits for fault
 * signals regarding a sub-region-manager session that is mapped into the local
 * address space as a dataspace. If a fault occurs, this thread allocates a new
 * dataspace and attaches it to the faulting address to resolve the fault. The
 * main thread performs memory accesses at the local address range that is
 * backed by the sub-region-manager session. Thereby, it triggers
 * region-manager faults.
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/env.h>
#include <base/thread.h>
#include <base/signal.h>
#include <cap_session/connection.h>
#include <rm_session/connection.h>
#include <dataspace/client.h>

using namespace Genode;


enum {
	MANAGED_SIZE = 0x00010000,
	PAGE_SIZE    = 4096,
};


/**
 * Region-manager fault handler resolves faults by attaching new dataspaces
 */
class Local_fault_handler : public Thread<4096>
{
	private:

		Rm_session      *_rm_session;
		Signal_receiver *_receiver;

	public:

		Local_fault_handler(Rm_session *rm_session, Signal_receiver *receiver)
		:
			Thread("local_fault_handler"),
			_rm_session(rm_session), _receiver(receiver)
		{ }

		void handle_fault()
		{
			Rm_session::State state = _rm_session->state();

			printf("rm session state is %s, pf_addr=0x%lx\n",
			       state.type == Rm_session::READ_FAULT  ? "READ_FAULT"  :
			       state.type == Rm_session::WRITE_FAULT ? "WRITE_FAULT" :
			       state.type == Rm_session::EXEC_FAULT  ? "EXEC_FAULT"  : "READY",
			       state.addr);

			printf("allocate dataspace and attach it to sub rm session\n");
			Dataspace_capability ds = env()->ram_session()->alloc(PAGE_SIZE);
			_rm_session->attach_at(ds, state.addr & ~(PAGE_SIZE - 1));

			printf("returning from handle_fault\n");
		}

		void entry()
		{
			while (true) {
				printf("fault handler: waiting for fault signal\n");
				Signal signal = _receiver->wait_for_signal();
				printf("received %u fault signals\n", signal.num());
				for (unsigned i = 0; i < signal.num(); i++)
					handle_fault();
			}
		}
};


int main(int argc, char **argv)
{
	printf("--- nested region-manager test ---\n");

	/*
	 * Initialize sub-region-manager session and set up a local fault handler
	 * for it.
	 */
	static Rm_connection sub_rm(0, MANAGED_SIZE);
	static Cap_connection cap;
	static Signal_receiver receiver;
	static Signal_context context;
	sub_rm.fault_handler(receiver.manage(&context));
	static Local_fault_handler fault_handler(&sub_rm, &receiver);
	fault_handler.start();

	/*
	 * Attach sub-region-manager session as dataspace to the local address
	 * space.
	 */
	void *addr = env()->rm_session()->attach(sub_rm.dataspace());

	printf("attached sub dataspace at local address 0x%p\n", addr);
	Dataspace_client client(sub_rm.dataspace());
	printf("sub dataspace size is %u should be %u\n", client.size(), MANAGED_SIZE);

	/*
	 * Walk through the address range belonging to the sub-region-manager session
	 */
	char *managed = (char *)addr;
	for (int i = 0; i < MANAGED_SIZE; i += PAGE_SIZE/16) {
		printf("write to %p\n", &managed[i]);
		managed[i] = 13;
	}

	printf("--- finished nested region-manager test ---\n");
	return 0;
}
