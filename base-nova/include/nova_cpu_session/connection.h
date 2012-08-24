/*
 * \brief  Connection to NOVA specific CPU service
 * \author Alexander Boettcher
 * \date   2012-07-27
 */

/*
 * Copyright (C) 2012-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOVA_CPU_SESSION__CONNECTION_H_
#define _INCLUDE__NOVA_CPU_SESSION__CONNECTION_H_

#include <cpu_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Nova_cpu_connection : Connection<Cpu_session>, Cpu_session_client
	{
		/**
		 * Constructor
		 *
		 * \param label     initial session label
		 * \param priority  designated priority of all threads created
		 *                  with this CPU session
		 */
		Nova_cpu_connection(const char *label    = "", long        priority = DEFAULT_PRIORITY)
		:
			Connection<Cpu_session>(
				session("priority=0x%lx, ram_quota=32K, label=\"%s\"",
				        priority, label)),
			Cpu_session_client(cap()) { }
	};
}

#endif /* _INCLUDE__NOVA_CPU_SESSION__CONNECTION_H_ */
