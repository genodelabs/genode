/*
 * \brief  Support for the Linux-specific environment
 * \author Norman Feske
 * \date   2008-12-12
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <base/thread.h>
#include <linux_dataspace/client.h>
#include <linux_syscalls.h>

/* local includes */
#include <platform_env.h>

using namespace Genode;


/****************************************************
 ** Support for Platform_env_base::Rm_session_mmap **
 ****************************************************/

Genode::size_t
Platform_env_base::Rm_session_mmap::_dataspace_size(Dataspace_capability ds)
{
	if (ds.valid())
		return Dataspace_client(ds).size();

	return Local_capability<Dataspace>::deref(ds)->size();
}


int Platform_env_base::Rm_session_mmap::_dataspace_fd(Dataspace_capability ds)
{
	return Linux_dataspace_client(ds).fd().dst().socket;
}


bool
Platform_env_base::Rm_session_mmap::_dataspace_writable(Dataspace_capability ds)
{
	return Dataspace_client(ds).writable();
}


/********************************
 ** Platform_env::Local_parent **
 ********************************/

static inline size_t get_page_size_log2() { return 12; }


Session_capability
Platform_env::Local_parent::session(Service_name const &service_name,
                                    Session_args const &args,
                                    Affinity     const &affinity)
{
	if (strcmp(service_name.string(),
	    Rm_session::service_name()) == 0)
	{
		size_t size =
			Arg_string::find_arg(args.string(),"size")
				.ulong_value(~0);

		if (size == 0)
			return Expanding_parent_client::session(service_name, args, affinity);

		if (size != ~0UL)
			size = align_addr(size, get_page_size_log2());

		Rm_session_mmap *rm = new (env()->heap())
		                      Rm_session_mmap(true, size);

		return Local_capability<Session>::local_cap(rm);
	}

	return Expanding_parent_client::session(service_name, args, affinity);
}


void Platform_env::Local_parent::close(Session_capability session)
{
	/*
	 * Handle non-local capabilities
	 */
	if (session.valid()) {
		Parent_client::close(session);
		return;
	}

	/*
	 * Detect capability to local RM session
	 */
	Capability<Rm_session_mmap> rm = static_cap_cast<Rm_session_mmap>(session);

	destroy(env()->heap(), Local_capability<Rm_session_mmap>::deref(rm));
}


Platform_env::Local_parent::Local_parent(Parent_capability parent_cap,
                                         Emergency_ram_reserve &reserve)
: Expanding_parent_client(parent_cap, reserve)
{ }


/******************
 ** Platform_env **
 ******************/

/**
 * List of Unix environment variables, initialized by the startup code
 */
extern char **lx_environ;


/**
 * Read environment variable as long value
 */
static unsigned long get_env_ulong(const char *key)
{
	for (char **curr = lx_environ; curr && *curr; curr++) {

		Arg arg = Arg_string::find_arg(*curr, key);
		if (arg.valid())
			return arg.ulong_value(0);
	}

	return 0;
}


static Parent_capability obtain_parent_cap()
{
	long local_name = get_env_ulong("parent_local_name");

	/* produce typed capability manually */
	typedef Native_capability::Dst Dst;
	Dst const dst(PARENT_SOCKET_HANDLE);
	return reinterpret_cap_cast<Parent>(Native_capability(dst, local_name));
}


Platform_env::Local_parent &Platform_env::_parent()
{
	static Local_parent local_parent(obtain_parent_cap(), *this);
	return local_parent;
}


Platform_env::Platform_env()
:
	Platform_env_base(static_cap_cast<Ram_session>(_parent().session("Env::ram_session", "")),
	                  static_cap_cast<Cpu_session>(_parent().session("Env::cpu_session", "")),
	                  static_cap_cast<Pd_session> (_parent().session("Env::pd_session",  ""))),
	_heap(Platform_env_base::ram_session(), Platform_env_base::rm_session()),
	_emergency_ram_ds(ram_session()->alloc(_emergency_ram_size()))
{
	/* register TID and PID of the main thread at core */
	cpu_session()->thread_id(parent()->main_thread_cap(),
	                         lx_getpid(), lx_gettid());
}


/*****************************
 ** Support for IPC library **
 *****************************/

namespace Genode {

	Native_connection_state server_socket_pair()
	{
		/*
		 * Obtain access to Linux-specific extension of the CPU session
		 * interface. We can cast to the specific type because the Linux
		 * version of 'Platform_env' is hosting a 'Linux_cpu_client' object.
		 */
		Linux_cpu_session *cpu = dynamic_cast<Linux_cpu_session *>(env()->cpu_session());

		if (!cpu) {
			PERR("could not obtain Linux extension to CPU session interface");
			struct Could_not_access_linux_cpu_session { };
			throw Could_not_access_linux_cpu_session();
		}

		Native_connection_state ncs;

		Thread_base *thread = Thread_base::myself();
		if (thread) {
			ncs.server_sd = cpu->server_sd(thread->cap()).dst().socket;
			ncs.client_sd = cpu->client_sd(thread->cap()).dst().socket;
		}
		return ncs;
	}

	void destroy_server_socket_pair(Native_connection_state const &ncs)
	{
		/* close local file descriptor if it is valid */
		if (ncs.server_sd != -1) lx_close(ncs.server_sd);
		if (ncs.client_sd != -1) lx_close(ncs.client_sd);
	}
}
