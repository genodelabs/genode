/*
 * \brief  Connection to VM a service
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VM_SESSION__CONNECTION_H_
#define _INCLUDE__VM_SESSION__CONNECTION_H_

#include <util/retry.h>
#include <vm_session/client.h>
#include <cpu_session/cpu_session.h>
#include <base/connection.h>

namespace Genode { struct Vm_connection; }


struct Genode::Vm_connection : Connection<Vm_session>, Vm_session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Vm_session> _session(Parent &parent, char const *label, long priority,
	                                unsigned long affinity)
	{
		return session(parent,
		               "priority=0x%lx, affinity=0x%lx, ram_quota=16M, cap_quota=800, label=\"%s\"",
		               priority, affinity, label);
	}

	/**
	 * Constructor
	 *
	 * \param label     initial session label
	 * \param priority  designated priority of the VM
	 * \param affinity  which physical CPU the VM should run on top of
	 */
	Vm_connection(Env &env, const char *label = "",
	              long priority = Cpu_session::DEFAULT_PRIORITY,
	              unsigned long affinity = 0)
	:
		Connection<Vm_session>(env, _session(env.parent(), label, priority, affinity)),
		Vm_session_client(cap())
	{ }

	template <typename FUNC>
	auto with_upgrade(FUNC func) -> decltype(func())
	{
		return Genode::retry<Genode::Out_of_ram>(
			[&] () {
				return Genode::retry<Genode::Out_of_caps>(
					[&] () { return func(); },
					[&] () { this->upgrade_caps(2); });
			},
			[&] () { this->upgrade_ram(4096); }
		);
	}
};

#endif /* _INCLUDE__VM_SESSION__CONNECTION_H_ */
