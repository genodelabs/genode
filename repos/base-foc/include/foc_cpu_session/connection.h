/*
 * \brief  Connection to Fiasco.OC specific cpu service
 * \author Stefan Kalkowski
 * \date   2011-04-04
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FOC_CPU_SESSION__CONNECTION_H_
#define _INCLUDE__FOC_CPU_SESSION__CONNECTION_H_

#include <foc_cpu_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Foc_cpu_connection : Connection<Cpu_session>, Foc_cpu_session_client
	{
		/**
		 * Constructor
		 *
		 * \param label     initial session label
		 * \param priority  designated priority of all threads created
		 *                  with this CPU session
		 */
		Foc_cpu_connection(const char *label    = "",
		                   long        priority = DEFAULT_PRIORITY)
		:
			Connection<Cpu_session>(
				session("priority=0x%lx, ram_quota=32K, label=\"%s\"",
				        priority, label)),
			Foc_cpu_session_client(cap()) { }
	};
}

#endif /* _INCLUDE__FOC_CPU_SESSION__CONNECTION_H_ */
