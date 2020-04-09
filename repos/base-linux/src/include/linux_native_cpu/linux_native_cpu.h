/*
 * \brief  Linux-specific extension of the CPU session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LINUX_NATIVE_CPU__LINUX_NATIVE_CPU_H_
#define _INCLUDE__LINUX_NATIVE_CPU__LINUX_NATIVE_CPU_H_

#include <base/rpc.h>
#include <cpu_session/cpu_session.h>

namespace Genode { struct Linux_native_cpu; }


struct Genode::Linux_native_cpu : Cpu_session::Native_cpu
{
	/**
	 * Register Linux PID and TID of the specified thread
	 */
	virtual void thread_id(Thread_capability, int pid, int tid) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_thread_id, void, thread_id, Thread_capability, int, int);

	GENODE_RPC_INTERFACE(Rpc_thread_id);
};

#endif /* _INCLUDE__LINUX_NATIVE_CPU__LINUX_NATIVE_CPU_H_ */
