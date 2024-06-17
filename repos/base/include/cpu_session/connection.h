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
	                                   Weight                 weight,
	                                   addr_t                 utcb = 0) override
	{
		Thread_capability result { };
		bool denied = false;
		while (!result.valid()) {
			using Error = Cpu_session::Create_thread_error;
			Cpu_session_client::create_thread(pd, name, affinity, weight, utcb).with_result(
				[&] (Thread_capability cap) { result = cap; },
				[&] (Error e) {
					switch (e) {
					case Error::OUT_OF_RAM:  upgrade_ram(8*1024); break;
					case Error::OUT_OF_CAPS: upgrade_caps(2);     break;
					case Error::DENIED:      denied = true;       break;
					}
				});
			if (denied)
				return Error::DENIED;
		}
		return result;
	}
};

#endif /* _INCLUDE__CPU_SESSION__CONNECTION_H_ */
