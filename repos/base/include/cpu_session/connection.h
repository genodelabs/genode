/*
 * \brief  Connection to CPU service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU_SESSION__CONNECTION_H_
#define _INCLUDE__CPU_SESSION__CONNECTION_H_

#include <cpu_session/client.h>
#include <base/connection.h>

namespace Genode { struct Cpu_connection; }


struct Genode::Cpu_connection : Connection<Cpu_session>, Cpu_session_client
{
	enum { RAM_QUOTA = 36*1024 };

	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Cpu_session> _session(Parent &parent, char const *label,
	                                 long priority, Affinity const &affinity)
	{
		return session(parent, affinity,
		               "priority=0x%lx, ram_quota=128K, label=\"%s\"",
		               priority, label);
	}

	/**
	 * Constructor
	 *
	 * \param label     initial session label
	 * \param priority  designated priority of all threads created
	 *                  with this CPU session
	 */
	Cpu_connection(Env &env, const char *label = "", long priority = DEFAULT_PRIORITY,
	               Affinity const &affinity = Affinity())
	:
		Connection<Cpu_session>(env, _session(env.parent(), label, priority, affinity)),
		Cpu_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Cpu_connection(const char *label = "", long priority = DEFAULT_PRIORITY,
	               Affinity const &affinity = Affinity()) __attribute__((deprecated))
	:
		Connection<Cpu_session>(_session(*env_deprecated()->parent(), label, priority, affinity)),
		Cpu_session_client(cap())
	{ }
};

#endif /* _INCLUDE__CPU_SESSION__CONNECTION_H_ */
