/*
 * \brief  Connection to CPU service
 * \author Martin Stein
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__CONNECTION_H_
#define _INCLUDE__CPU_SESSION__CONNECTION_H_

/* Genode includes */
#include <cpu_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Cpu_connection : Connection<Cpu_session>, Cpu_session_client
	{
		enum { RAM_QUOTA = 128*1024 };

		/**
		 * Constructor
		 *
		 * \param label     initial session label
		 * \param priority  designated priority of all threads created
		 *                  with this CPU session
		 */
		Cpu_connection(const char *label = "", long priority = DEFAULT_PRIORITY)
		:
			Connection<Cpu_session>(
				session("priority=0x%lx, ram_quota=128K, label=\"%s\"",
				        priority, label)),
			Cpu_session_client(cap()) { }
	};
}

#endif /* _INCLUDE__CPU_SESSION__CONNECTION_H_ */
