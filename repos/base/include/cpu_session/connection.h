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
	 * Constructor
	 *
	 * \param label     initial session label
	 * \param priority  designated priority of all threads created
	 *                  with this CPU session
	 */
	Cpu_connection(Env &env, const char *label = "", long priority = DEFAULT_PRIORITY,
	               Affinity const &affinity = Affinity())
	:
		Connection<Cpu_session>(env,
		                        session(env.parent(), affinity,
		                                "priority=0x%lx, ram_quota=%u, "
		                                "cap_quota=%u, label=\"%s\"",
		                                priority, RAM_QUOTA, CAP_QUOTA, label)),
		Cpu_session_client(cap())
	{ }

	Thread_capability create_thread(Capability<Pd_session> pd,
	                                Name const            &name,
	                                Affinity::Location     affinity,
	                                Weight                 weight,
	                                addr_t                 utcb = 0) override
	{
		return retry_with_upgrade(Ram_quota{8*1024}, Cap_quota{2}, [&] () {
			return Cpu_session_client::create_thread(pd, name, affinity, weight, utcb); });
	}
};

#endif /* _INCLUDE__CPU_SESSION__CONNECTION_H_ */
