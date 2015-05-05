/*
 * \brief  Test program for raising and handling region-manager faults
 * \author Norman Feske
 * \date   2008-09-24
 *
 * This program starts itself as child. When started, it first determines
 * wheather it is parent or child by requesting its own file from the ROM
 * service. Because the program blocks all session-creation calls for the
 * ROM service, each program instance can determine its parent or child
 * role by the checking the result of the session creation.
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/child.h>
#include <pd_session/connection.h>
#include <rm_session/connection.h>
#include <ram_session/connection.h>
#include <rom_session/connection.h>
#include <cpu_session/connection.h>
#include <cap_session/connection.h>
#include <rm_session/client.h>

using namespace Genode;

/***********
 ** Child **
 ***********/

enum { MANAGED_ADDR = 0x10000000 };


void read_at(addr_t addr)
{
	printf("perform read operation at 0x%p\n", (void *)addr);
	int value = *(int *)addr;
	printf("read value %x\n", value);
}

void modify(addr_t addr)
{
	printf("modify memory at 0x%p to %x\n", (void *)addr, ++(*(int *)addr));
}

void main_child()
{
	printf("child role started\n");

	/* perform illegal access */
	read_at(MANAGED_ADDR);

	while (true)
		modify(MANAGED_ADDR);

	printf("--- child role of region-manager fault test finished ---\n");
}


/************
 ** Parent **
 ************/

class Test_child : public Child_policy
{
	private:

		enum { STACK_SIZE = 8*1024 };

		/*
		 * Entry point used for serving the parent interface
		 */
		Rpc_entrypoint _entrypoint;

		Child _child;

		Parent_service _log_service;
		Parent_service _rm_service;

	public:

		/**
		 * Constructor
		 */
		Test_child(Genode::Dataspace_capability    elf_ds,
		           Genode::Pd_session_capability   pd,
		           Genode::Ram_session_capability  ram,
		           Genode::Cpu_session_capability  cpu,
		           Genode::Rm_session_capability   rm,
		           Genode::Cap_session            *cap)
		:
			_entrypoint(cap, STACK_SIZE, "child", false),
			_child(elf_ds, pd, ram, cpu, rm, &_entrypoint, this),
			_log_service("LOG"), _rm_service("RM")
		{
			/* start execution of the new child */
			_entrypoint.activate();
		}

		Rm_session_capability rm_session_cap() { return _child.rm_session_cap(); }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return "rmchild"; }

		Service *resolve_session_request(const char *service, const char *)
		{
			/* forward white-listed session requests to our parent */
			return !strcmp(service, "LOG") ? &_log_service
			     : !strcmp(service, "RM")  ? &_rm_service
			     : 0;
		}

		void filter_session_args(const char *service,
		                         char *args, size_t args_len)
		{
			/* define session label for sessions forwarded to our parent */
			Arg_string::set_arg(args, args_len, "label", "child");
		}
};


void main_parent(Dataspace_capability elf_ds)
{
	printf("parent role started\n");

	/* create environment for new child */
	static Pd_connection  pd;
	static Ram_connection ram;
	static Cpu_connection cpu;
	static Rm_connection  rm;
	static Cap_connection cap;

	/* transfer some of our own ram quota to the new child */
	enum { CHILD_QUOTA = 1*1024*1024 };
	ram.ref_account(env()->ram_session_cap());
	env()->ram_session()->transfer_quota(ram.cap(), CHILD_QUOTA);

	static Signal_receiver fault_handler;

	/* register fault handler */
	static Signal_context signal_context;
	rm.fault_handler(fault_handler.manage(&signal_context));

	/* create child */
	static Test_child child(elf_ds, pd.cap(), ram.cap(), cpu.cap(),
	                        rm.cap(), &cap);

	/* allocate dataspace used for creating shared memory between parent and child */
	Dataspace_capability ds = env()->ram_session()->alloc(4096);
	volatile int *local_addr = env()->rm_session()->attach(ds);

	for (int i = 0; i < 4; i++) {

		printf("wait for region-manager fault\n");
		fault_handler.wait_for_signal();
		printf("received region-manager fault signal, request fault state\n");

		Rm_session::State state = rm.state();

		printf("rm session state is %s, pf_addr=0x%p\n",
		       state.type == Rm_session::READ_FAULT  ? "READ_FAULT"  :
		       state.type == Rm_session::WRITE_FAULT ? "WRITE_FAULT" :
		       state.type == Rm_session::EXEC_FAULT  ? "EXEC_FAULT"  : "READY",
		       (void *)state.addr);

		/* ignore spuriuous fault signal */
		if (state.type == Rm_session::READY) {
			PINF("ignoring spurious fault signal");
			continue;
		}

		addr_t child_virt_addr = state.addr & ~(4096 - 1);

		/* allocate dataspace to resolve the fault */
		printf("attach dataspace to the child at 0x%p\n", (void *)child_virt_addr);
		*local_addr = 0x1234;

		rm.attach_at(ds, child_virt_addr);

		/* wait until our child modifies the dataspace content */
		while (*local_addr == 0x1234);

		printf("child modified dataspace content, new value is %x\n", *local_addr);

		printf("revoke dataspace from child\n");
		rm.detach((void *)child_virt_addr);
	}

	fault_handler.dissolve(&signal_context);

	printf("--- parent role of region-manager fault test finished ---\n");
}


/*************************
 ** Common main program **
 *************************/

int main(int argc, char **argv)
{
	printf("--- region-manager fault test ---\n");

	/* obtain own elf file from rom service */
	try {
		static Rom_connection rom("test-rm_fault");
		main_parent(rom.dataspace());
	} catch (Genode::Rom_connection::Rom_connection_failed) {
		main_child();
	}

	return 0;
}
