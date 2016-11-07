/*
 * \brief  Test program for detecting faults
 * \author Norman Feske
 * \date   2013-01-03
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
#include <ram_session/connection.h>
#include <rom_session/connection.h>
#include <cpu_session/connection.h>
#include <cap_session/connection.h>
#include <pd_session/connection.h>
#include <loader_session/connection.h>
#include <region_map/client.h>


/***************
 ** Utilities **
 ***************/

static void wait_for_signal_for_context(Genode::Signal_receiver &sig_rec,
                                        Genode::Signal_context const &sig_ctx)
{
	Genode::Signal s = sig_rec.wait_for_signal();

	if (s.num() && s.context() == &sig_ctx) {
		Genode::log("got exception for child");
	} else {
		Genode::error("got unexpected signal while waiting for child");
		class Unexpected_signal { };
		throw Unexpected_signal();
	}
}


/******************************************************************
 ** Test for detecting the failure of an immediate child process **
 ******************************************************************/

class Test_child : public Genode::Child_policy
{
	private:

		struct Resources
		{
			Genode::Pd_connection  pd;
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;

			Resources(Genode::Signal_context_capability sigh, char const *label)
			: pd(label)
			{
				using namespace Genode;

				/* transfer some of our own ram quota to the new child */
				enum { CHILD_QUOTA = 1*1024*1024 };
				ram.ref_account(env()->ram_session_cap());
				env()->ram_session()->transfer_quota(ram.cap(), CHILD_QUOTA);

				/* register default exception handler */
				cpu.exception_sigh(sigh);

				/* register handler for unresolvable page faults */
				Region_map_client address_space(pd.address_space());
				address_space.fault_handler(sigh);
			}
		} _resources;

		Genode::Child::Initial_thread _initial_thread;

		/*
		 * The order of the following members is important. The services must
		 * appear before the child to ensure the correct order of destruction.
		 * I.e., the services must remain alive until the child has stopped
		 * executing. Otherwise, the child may hand out already destructed
		 * local services when dispatching an incoming session call.
		 */
		Genode::Rom_connection    _elf;
		Genode::Parent_service    _log_service;
		Genode::Parent_service    _rm_service;
		Genode::Region_map_client _address_space { _resources.pd.address_space() };
		Genode::Child             _child;

	public:

		/**
		 * Constructor
		 */
		Test_child(Genode::Rpc_entrypoint           &ep,
		           char const                       *elf_name,
		           Genode::Signal_context_capability sigh)
		:
			_resources(sigh, elf_name),
			_initial_thread(_resources.cpu, _resources.pd, elf_name),
			_elf(elf_name),
			_log_service("LOG"), _rm_service("RM"),
			_child(_elf.dataspace(), Genode::Dataspace_capability(),
			       _resources.pd,  _resources.pd,
			       _resources.ram, _resources.ram,
			       _resources.cpu, _initial_thread,
			       *Genode::env()->rm_session(), _address_space, ep, *this)
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return "child"; }

		Genode::Service *resolve_session_request(const char *service, const char *)
		{
			/* forward white-listed session requests to our parent */
			return !Genode::strcmp(service, "LOG") ? &_log_service
			     : !Genode::strcmp(service, "RM")  ? &_rm_service
			     : 0;
		}

		void filter_session_args(const char *service,
		                         char *args, Genode::size_t args_len)
		{
			/* define session label for sessions forwarded to our parent */
			Genode::Arg_string::set_arg_string(args, args_len, "label", "child");
		}
};


void faulting_child_test()
{
	using namespace Genode;

	log("-- exercise failure detection of immediate child --");

	/*
	 * Entry point used for serving the parent interface
	 */
	enum { STACK_SIZE = 8*1024 };
	Cap_connection cap;
	Rpc_entrypoint ep(&cap, STACK_SIZE, "child");

	/*
	 * Signal receiver and signal context for signals originating from the
	 * children's CPU-session and RM session.
	 */
	Signal_receiver sig_rec;
	Signal_context  sig_ctx;

	/*
	 * Iteratively start a faulting program and detect the faults
	 */
	for (int i = 0; i < 5; i++) {

		log("create child ", i);

		/* create and start child process */
		Test_child child(ep, "test-segfault", sig_rec.manage(&sig_ctx));

		log("wait_for_signal");


		wait_for_signal_for_context(sig_rec, sig_ctx);

		sig_rec.dissolve(&sig_ctx);

		/*
		 * When finishing the loop iteration, the local variables including
		 * 'child' will get destructed. A new child will be created at the
		 * beginning of the next iteration.
		 */
	}

	log("");
}


/******************************************************************
 ** Test for detecting failures in a child started by the loader **
 ******************************************************************/

void faulting_loader_child_test()
{
	using namespace Genode;

	log("-- exercise failure detection of loaded child --");

	/*
	 * Signal receiver and signal context for receiving faults originating from
	 * the loader subsystem.
	 */
	static Signal_receiver sig_rec;
	Signal_context sig_ctx;

	for (int i = 0; i < 5; i++) {

		log("create loader session ", i);

		Loader::Connection loader(1024*1024);

		/* register fault handler at loader session */
		loader.fault_sigh(sig_rec.manage(&sig_ctx));

		/* start subsystem */
		loader.start("test-segfault");

		wait_for_signal_for_context(sig_rec, sig_ctx);

		sig_rec.dissolve(&sig_ctx);
	}

	log("");
}


/***********************************************************************
 ** Test for detecting failures in a grandchild started by the loader **
 ***********************************************************************/

void faulting_loader_grand_child_test()
{
	using namespace Genode;

	log("-- exercise failure detection of loaded grand child --");

	/*
	 * Signal receiver and signal context for receiving faults originating from
	 * the loader subsystem.
	 */
	static Signal_receiver sig_rec;
	Signal_context sig_ctx;

	for (int i = 0; i < 5; i++) {

		log("create loader session ", i);

		Loader::Connection loader(2024*1024);

		/*
		 * Install init config for subsystem into the loader session
		 */
		char const *config =
			"<config>\n"
			"  <parent-provides>\n"
			"    <service name=\"ROM\"/>\n"
			"    <service name=\"RM\"/>\n"
			"    <service name=\"LOG\"/>\n"
			"  </parent-provides>\n"
			"  <default-route>\n"
			"    <any-service> <parent/> <any-child/> </any-service>\n"
			"  </default-route>\n"
			"  <start name=\"test-segfault\">\n"
			"    <resource name=\"RAM\" quantum=\"10M\"/>\n"
			"  </start>\n"
			"</config>";

		size_t config_size = strlen(config);

		Dataspace_capability config_ds =
			loader.alloc_rom_module("config", config_size);

		char *config_ds_addr = env()->rm_session()->attach(config_ds);
		memcpy(config_ds_addr, config, config_size);
		env()->rm_session()->detach(config_ds_addr);

		loader.commit_rom_module("config");

		/* register fault handler at loader session */
		loader.fault_sigh(sig_rec.manage(&sig_ctx));

		/* start subsystem */
		loader.start("init", "init");

		wait_for_signal_for_context(sig_rec, sig_ctx);

		sig_rec.dissolve(&sig_ctx);
	}

	log("");
}


/******************
 ** Main program **
 ******************/

int main(int argc, char **argv)
{
	using namespace Genode;

	log("--- fault_detection test started ---");

	faulting_child_test();

	faulting_loader_child_test();

	faulting_loader_grand_child_test();

	log("--- finished fault_detection test ---");
	return 0;
}

