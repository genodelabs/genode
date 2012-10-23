/*
 * \brief  Connection to VM a service
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VM_SESSION__CONNECTION_H_
#define _INCLUDE__VM_SESSION__CONNECTION_H_

#include <vm_session/client.h>
#include <cpu_session/cpu_session.h>
#include <base/connection.h>

namespace Genode
{
	/**
	 * Connection to a VM service
	 */
	struct Vm_connection : Connection<Vm_session>, Vm_session_client
	{
		/**
		 * Constructor
		 *
		 * \param label     initial session label
		 * \param priority  designated priority of the VM
		 * \param affinity  which physical CPU the VM should run on top of
		 */
		Vm_connection(const char *label = "",
		              long priority = Cpu_session::DEFAULT_PRIORITY,
		              unsigned long affinity = 0)
		: Connection<Vm_session>(
			session("priority=0x%lx, affinity=0x%lx, ram_quota=16K, label=\"%s\"",
			        priority, affinity, label)),
		  Vm_session_client(cap()) { }
	};
}

#endif /* _INCLUDE__VM_SESSION__CONNECTION_H_ */
