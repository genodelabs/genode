/*
 * \brief  Connection to PD service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PD_SESSION__CONNECTION_H_
#define _INCLUDE__PD_SESSION__CONNECTION_H_

#include <pd_session/client.h>
#include <base/connection.h>

namespace Genode { struct Pd_connection; }


struct Genode::Pd_connection : Connection<Pd_session>, Pd_session_client
{
	enum { RAM_QUOTA = 24*1024*sizeof(long)};
	enum Virt_space { UNCONSTRAIN = 0, CONSTRAIN = 1 };

	/**
	 * Constructor
	 *
	 * \param label  session label
	 */
	Pd_connection(Env &env, char const *label = "", Virt_space space = CONSTRAIN)
	:
		Connection<Pd_session>(env, session(env.parent(),
		                                    "ram_quota=%u, cap_quota=%u, label=\"%s\", virt_space=%u",
		                                    RAM_QUOTA, CAP_QUOTA, label, space)),
		Pd_session_client(cap())
	{ }

	struct Device_pd { };

	/**
	 * Constructor used for creating device protection domains
	 */
	Pd_connection(Env &env, Device_pd)
	:
		Connection<Pd_session>(env, session(env.parent(),
		                                    "ram_quota=%u, cap_quota=%u, "
		                                    "label=\"device PD\", virt_space=%u, "
		                                    "managing_system=yes",
		                                    RAM_QUOTA, CAP_QUOTA, UNCONSTRAIN)),
		Pd_session_client(cap())
	{ }
};

#endif /* _INCLUDE__PD_SESSION__CONNECTION_H_ */
