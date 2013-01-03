/*
 * \brief  Test program for failsafe monitoring
 * \author Norman Feske
 * \date   2013-01-03
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/child.h>
#include <ram_session/connection.h>
#include <rom_session/connection.h>
#include <cpu_session/connection.h>
#include <cap_session/connection.h>


class Test_child : public Genode::Child_policy
{
	private:

		struct Resources
		{
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;
			Genode::Rm_connection  rm;

			Resources(Genode::Signal_context_capability sigh)
			{
				using namespace Genode;

				/* transfer some of our own ram quota to the new child */
				enum { CHILD_QUOTA = 1*1024*1024 };
				ram.ref_account(env()->ram_session_cap());
				env()->ram_session()->transfer_quota(ram.cap(), CHILD_QUOTA);

				/*
				 * Register default exception handler by specifying an invalid
				 * thread capability.
				 */
				cpu.exception_handler(Thread_capability(), sigh);
			}
		} _resources;

		Genode::Rom_connection _elf;

		Genode::Child _child;

		Genode::Parent_service _log_service;

	public:

		/**
		 * Constructor
		 */
		Test_child(Genode::Rpc_entrypoint           &ep,
		           char const                       *elf_name,
		           Genode::Signal_context_capability sigh)
		:
			_resources(sigh),
			_elf(elf_name),
			_child(_elf.dataspace(), _resources.ram.cap(),
			       _resources.cpu.cap(), _resources.rm.cap(), &ep, this),
			_log_service("LOG")
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return "child"; }

		Genode::Service *resolve_session_request(const char *service, const char *)
		{
			/* forward log-session request to our parent */
			return !Genode::strcmp(service, "LOG") ? &_log_service : 0;
		}

		void filter_session_args(const char *service,
		                         char *args, Genode::size_t args_len)
		{
			/* define session label for sessions forwarded to our parent */
			Genode::Arg_string::set_arg(args, args_len, "label", "child");
		}
};


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- failsafe test started ---\n");

	/*
	 * Entry point used for serving the parent interface
	 */
	enum { STACK_SIZE = 8*1024 };
	Cap_connection cap;
	Rpc_entrypoint ep(&cap, STACK_SIZE, "child");

	/*
	 * Signal receiver of CPU-session exception signals
	 */
	static Signal_receiver sig_rec;

	for (int i = 0; i < 5; i++) {

		PLOG("create child %d", i);

		Signal_context sig_ctx;
		Signal_context_capability exception_sigh = sig_rec.manage(&sig_ctx);

		Test_child child(ep, "test-segfault", exception_sigh);

		Signal s = sig_rec.wait_for_signal();

		if (s.num() && s.context() == &sig_ctx) {
			PLOG("got exception for child %d", i);
		} else {
			PERR("got unexpected signal while waiting for child %d", i);
			return -2;
		}

		sig_rec.dissolve(&sig_ctx);

		/*
		 * When finishing the loop iteration, the local variables including
		 * 'child' will get destructed. A new child will be created at the
		 * beginning of the next iteration.
		 */
	}

	printf("--- finished failsafe test ---\n");
	return 0;
}

