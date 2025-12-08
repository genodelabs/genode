/*
 * \brief  Connection to CPU service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
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
	/**
	 * Constructor
	 *
	 * \param priority  designated priority of all threads created
	 *                  with this CPU session
	 */
	Cpu_connection(Env            &env,
	               Label    const &label    = Label(),
	               long            priority = DEFAULT_PRIORITY,
	               Affinity const &affinity = Affinity())
	:
		Connection<Cpu_session>(env, label, Ram_quota { RAM_QUOTA }, affinity,
		                        Args("priority=", Hex(priority))),
		Cpu_session_client(cap())
	{ }

	Create_thread_result create_thread(Capability<Pd_session> pd,
	                                   Name const            &name,
	                                   Affinity::Location     affinity,
	                                   addr_t                 utcb = 0) override
	{
		return retry(Ram_quota{8*1024}, Cap_quota{2}, [&] {
			return Cpu_session_client::create_thread(pd, name, affinity, utcb);
		});
	}
};

#endif /* _INCLUDE__CPU_SESSION__CONNECTION_H_ */
