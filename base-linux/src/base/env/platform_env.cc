/*
 * \brief  Support for the Linux-specific environment
 * \author Norman Feske
 * \date   2008-12-12
 */

/*
 * Copyright (C) 2008-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/arg_string.h>
#include <base/platform_env.h>

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

		return Local_interface::capability(rm);
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
	try {
		Capability<Rm_session_mmap> rm =
			static_cap_cast<Rm_session_mmap>(session);

		destroy(env()->heap(), Local_interface::deref(rm));

	} catch (Local_interface::Non_local_capability) { }
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
