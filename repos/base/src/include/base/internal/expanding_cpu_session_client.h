/*
 * \brief  CPU-session client that upgrades its session quota on demand
 * \author Norman Feske
 * \date   2006-07-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__EXPANDING_CPU_SESSION_CLIENT_H_
#define _INCLUDE__BASE__INTERNAL__EXPANDING_CPU_SESSION_CLIENT_H_

/* Genode includes */
#include <util/retry.h>
#include <cpu_session/client.h>

/* base-internal includes */
#include <base/internal/upgradeable_client.h>


namespace Genode { struct Expanding_cpu_session_client; }


struct Genode::Expanding_cpu_session_client : Upgradeable_client<Genode::Cpu_session_client>
{
	Expanding_cpu_session_client(Parent &parent, Genode::Cpu_session_capability cap, Parent::Client::Id id)
	:
		/*
		 * We need to upcast the capability because on some platforms (i.e.,
		 * NOVA), 'Cpu_session_client' refers to a platform-specific session
		 * interface ('Nova_cpu_session').
		 */
		Upgradeable_client<Genode::Cpu_session_client>
			(parent, static_cap_cast<Genode::Cpu_session_client::Rpc_interface>(cap), id)
	{ }

	Create_thread_result
	create_thread(Pd_session_capability pd, Name const &name,
	              Affinity::Location location, Weight weight, addr_t utcb) override
	{
		Thread_capability result { };
		bool denied = false;
		while (!result.valid()) {
			using Error = Cpu_session::Create_thread_error;
			Cpu_session_client::create_thread(pd, name, location, weight, utcb).with_result(
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

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_CPU_SESSION_CLIENT_H_ */
