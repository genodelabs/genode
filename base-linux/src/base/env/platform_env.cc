/*
 * \brief  Support for the Linux-specific environment
 * \author Norman Feske
 * \date   2008-12-12
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/arg_string.h>
#include <base/platform_env.h>
#include <base/thread.h>

using namespace Genode;

/********************************
 ** Platform_env::Local_parent **
 ********************************/

Session_capability
Platform_env::Local_parent::session(Service_name const &service_name,
                                    Session_args const &args)
{
	if (strcmp(service_name.string(),
	    Rm_session::service_name()) == 0)
	{
		size_t size =
			Arg_string::find_arg(args.string(),"size")
				.ulong_value(~0);

		if (size == 0)
			return Parent_client::session(service_name, args);

		Rm_session_mmap *rm = new (env()->heap())
		                      Rm_session_mmap(true, size);

		return Session_capability::local_cap(rm);
	}

	return Parent_client::session(service_name, args);
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

	destroy(env()->heap(), Capability<Rm_session_mmap>::deref(rm));
}


Platform_env::Local_parent::Local_parent(Parent_capability parent_cap)
: Parent_client(parent_cap) { }


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
unsigned long Platform_env::_get_env_ulong(const char *key)
{
	for (char **curr = lx_environ; curr && *curr; curr++) {

		Arg arg = Arg_string::find_arg(*curr, key);
		if (arg.valid())
			return arg.ulong_value(0);
	}

	return 0;
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
}
