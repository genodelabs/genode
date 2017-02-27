/*
 * \brief  NOVA-specific part of the CPU session interface
 * \author Norman Feske
 * \date   2016-04-21
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA_NATIVE_CPU__FOC_NATIVE_CPU_H_
#define _INCLUDE__NOVA_NATIVE_CPU__FOC_NATIVE_CPU_H_

#include <base/rpc.h>
#include <cpu_session/cpu_session.h>

namespace Genode { struct Nova_native_cpu; }


struct Genode::Nova_native_cpu : Cpu_session::Native_cpu
{
	enum Thread_type { GLOBAL, LOCAL, VCPU };

	/*
	 * Exception base of thread in caller protection domain - not in core!
	 */
	struct Exception_base { addr_t exception_base; };


	virtual void thread_type(Thread_capability, Thread_type, Exception_base) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_thread_type, void, thread_type, Thread_capability,
	           Thread_type, Exception_base );
	GENODE_RPC_INTERFACE(Rpc_thread_type);
};

#endif /* _INCLUDE__NOVA_NATIVE_CPU__FOC_NATIVE_CPU_H_ */
