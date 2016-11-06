/*
 * \brief  CPU-session client that upgrades its session quota on demand
 * \author Norman Feske
 * \date   2006-07-28
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
	Expanding_cpu_session_client(Genode::Cpu_session_capability cap, Parent::Client::Id id)
	:
		/*
		 * We need to upcast the capability because on some platforms (i.e.,
		 * NOVA), 'Cpu_session_client' refers to a platform-specific session
		 * interface ('Nova_cpu_session').
		 */
		Upgradeable_client<Genode::Cpu_session_client>
			(static_cap_cast<Genode::Cpu_session_client::Rpc_interface>(cap), id)
	{ }

	Thread_capability
	create_thread(Pd_session_capability pd, Name const &name,
	              Affinity::Location location, Weight weight, addr_t utcb) override
	{
		return retry<Cpu_session::Out_of_metadata>(
			[&] () {
				return Cpu_session_client::create_thread(pd, name, location,
				                                         weight, utcb); },
			[&] () { upgrade_ram(8*1024); });
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_CPU_SESSION_CLIENT_H_ */
