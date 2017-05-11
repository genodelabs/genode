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

	/**
	 * Constructor
	 *
	 * \param label  session label
	 */
	Pd_connection(Env &env, char const *label = "")
	:
		Connection<Pd_session>(env, session(env.parent(),
		                                    "ram_quota=%u, cap_quota=%u, label=\"%s\"",
		                                    RAM_QUOTA, CAP_QUOTA, label)),
		Pd_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Pd_connection(char const *label = "") __attribute__((deprecated))
	:
		Connection<Pd_session>(session("ram_quota=%u, label=\"%s\"", RAM_QUOTA, label)),
		Pd_session_client(cap())
	{ }
};

#endif /* _INCLUDE__PD_SESSION__CONNECTION_H_ */
