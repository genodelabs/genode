/*
 * \brief  Linux-specific extension of the CPU session interface
 * \author Norman Feske
 * \date   2012-08-09
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LINUX_CPU_SESSION__LINUX_CPU_SESSION_H_
#define _INCLUDE__LINUX_CPU_SESSION__LINUX_CPU_SESSION_H_

#include <cpu_session/cpu_session.h>

namespace Genode {

	struct Linux_cpu_session : Cpu_session
	{
		virtual ~Linux_cpu_session() { }

		/**
		 * Register Linux PID and TID of the specified thread
		 */
		virtual void thread_id(Thread_capability, int pid, int tid) = 0;

		/*
		 * If a thread plays the role of an entrypoint, core creates a bound
		 * socket pair for the thread and passes both makes the socket
		 * descriptors of both ends available to the owner of the thread's
		 * CPU session via the 'server_sd' and 'client_sd' function.
		 */

		/**
		 * Request server-side socket descriptor
		 *
		 * The socket descriptor returned by this function is meant to be used
		 * exclusively by the server for receiving incoming requests. It should
		 * never leave the server process.
		 */
		virtual Untyped_capability server_sd(Thread_capability thread) = 0;

		/**
		 * Request client-side socket descriptor
		 *
		 * The returned socket descriptor enables a client to send messages to
		 * the thread. It is already connected to the 'server_sd' descriptor.
		 * In contrast to 'server_sd', the 'client_sd' is expected to be passed
		 * around via capability delegations.
		 */
		virtual Untyped_capability client_sd(Thread_capability thread) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_thread_id, void, thread_id, Thread_capability, int, int);
		GENODE_RPC(Rpc_server_sd, Untyped_capability, server_sd, Thread_capability);
		GENODE_RPC(Rpc_client_sd, Untyped_capability, client_sd, Thread_capability);

		GENODE_RPC_INTERFACE_INHERIT(Cpu_session,
		                             Rpc_thread_id, Rpc_server_sd, Rpc_client_sd);
	};
}

#endif /* _INCLUDE__LINUX_CPU_SESSION__LINUX_CPU_SESSION_H_ */
