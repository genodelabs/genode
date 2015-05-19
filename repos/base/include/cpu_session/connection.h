/*
 * \brief  Connection to CPU service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
	 * Constructor
	 *
	 * \param label     initial session label
	 * \param priority  designated priority of all threads created
	 *                  with this CPU session
	 */
	Cpu_connection(char     const *label    = "",
	               long            priority = DEFAULT_PRIORITY,
	               Affinity const &affinity = Affinity())
	:
		Connection<Cpu_session>(
			session(affinity, "priority=0x%lx, ram_quota=%u, label=\"%s\"",
			        priority, RAM_QUOTA, label)),
		Cpu_session_client(cap()) { }
};

#endif /* _INCLUDE__CPU_SESSION__CONNECTION_H_ */
