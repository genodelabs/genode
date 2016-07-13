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

#include <base/log.h>
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
	log("perform read operation at ", Hex(addr));
	int value = *(int *)addr;
	log("read value ", Hex(value));
}

void modify(addr_t addr)
{
	log("modify memory at ", Hex(addr), " to ", Hex(++(*(int *)addr)));
}

void main_child()
{
	log("child role started");

	/* perform illegal access */
	read_at(MANAGED_ADDR);

	while (true)
		modify(MANAGED_ADDR);

	log("--- child role of region-manager fault test finished ---");
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

		Region_map_client     _address_space;
		Pd_session_client     _pd;
		Ram_session_client    _ram;
		Cpu_session_client    _cpu;
		Child::Initial_thread _initial_thread;

		Child _child;

		Parent_service _log_service;

	public:

		/**
		 * Constructor
		 */
		Test_child(Genode::Dataspace_capability    elf_ds,
		           Genode::Pd_connection          &pd,
		           Genode::Ram_session_capability  ram,
		           Genode::Cpu_session_capability  cpu,
		           Genode::Cap_session            *cap)
		:
			_entrypoint(cap, STACK_SIZE, "child", false),
			_address_space(pd.address_space()), _pd(pd), _ram(ram), _cpu(cpu),
			_initial_thread(_cpu, _pd, "child"),
			_child(elf_ds, Dataspace_capability(), _pd, _pd, _ram, _ram,
			       _cpu, _initial_thread, *env()->rm_session(), _address_space,
			       _entrypoint, *this),
			_log_service("LOG")
		{
			/* start execution of the new child */
			_entrypoint.activate();
		}


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return "rmchild"; }

		Service *resolve_session_request(const char *service, const char *)
		{
			/* forward white-listed session requests to our parent */
			return !strcmp(service, "LOG") ? &_log_service : 0;
		}

		void filter_session_args(const char *service,
		                         char *args, size_t args_len)
		{
			/* define session label for sessions forwarded to our parent */
			Arg_string::set_arg_string(args, args_len, "label", "child");
		}
};


void main_parent(Dataspace_capability elf_ds)
{
	log("parent role started");

	/* create environment for new child */
	static Pd_connection  pd;
	static Ram_connection ram;
	static Cpu_connection cpu;
	static Cap_connection cap;

	/* transfer some of our own ram quota to the new child */
	enum { CHILD_QUOTA = 1*1024*1024 };
	ram.ref_account(env()->ram_session_cap());
	env()->ram_session()->transfer_quota(ram.cap(), CHILD_QUOTA);

	static Signal_receiver fault_handler;

	/* register fault handler at the child's address space */
	static Signal_context signal_context;
	Region_map_client address_space(pd.address_space());
	address_space.fault_handler(fault_handler.manage(&signal_context));

	/* create child */
	static Test_child child(elf_ds, pd, ram.cap(), cpu.cap(), &cap);

	/* allocate dataspace used for creating shared memory between parent and child */
	Dataspace_capability ds = env()->ram_session()->alloc(4096);
	volatile int *local_addr = env()->rm_session()->attach(ds);

	for (int i = 0; i < 4; i++) {

		log("wait for region-manager fault");
		fault_handler.wait_for_signal();
		log("received region-manager fault signal, request fault state");

		Region_map::State state = address_space.state();

		char const *state_name =
			state.type == Region_map::State::READ_FAULT  ? "READ_FAULT"  :
			state.type == Region_map::State::WRITE_FAULT ? "WRITE_FAULT" :
			state.type == Region_map::State::EXEC_FAULT  ? "EXEC_FAULT"  : "READY";

		log("rm session state is ", state_name, ", pf_addr=", Hex(state.addr));

		/* ignore spuriuous fault signal */
		if (state.type == Region_map::State::READY) {
			log("ignoring spurious fault signal");
			continue;
		}

		addr_t child_virt_addr = state.addr & ~(4096 - 1);

		/* allocate dataspace to resolve the fault */
		log("attach dataspace to the child at ", Hex(child_virt_addr));
		*local_addr = 0x1234;

		address_space.attach_at(ds, child_virt_addr);

		/* wait until our child modifies the dataspace content */
		while (*local_addr == 0x1234);

		log("child modified dataspace content, new value is ", Hex(*local_addr));

		log("revoke dataspace from child");
		address_space.detach((void *)child_virt_addr);
	}

	fault_handler.dissolve(&signal_context);

	log("--- parent role of region-manager fault test finished ---");
}


/*************************
 ** Common main program **
 *************************/

int main(int argc, char **argv)
{
	log("--- region-manager fault test ---");

	/* obtain own elf file from rom service */
	try {
		static Rom_connection rom("test-rm_fault");
		main_parent(rom.dataspace());
	} catch (Genode::Rom_connection::Rom_connection_failed) {
		main_child();
	}

	return 0;
}
