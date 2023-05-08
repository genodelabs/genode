/*
 * \brief  NOVA-specific part of the CPU session interface
 * \author Norman Feske
 * \date   2022-06-02
 *
 * Mirrored from 'base-nova/include/nova_native_cpu/nova_native_cpu.h'.
 */

/*
 * Copyright (C) 2016-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NATIVE_CPU_NOVA_H_
#define _NATIVE_CPU_NOVA_H_

#include <base/rpc.h>

/* local includes */
#include <types.h>

namespace Monitor { struct Native_cpu_nova; }


struct Monitor::Native_cpu_nova : Interface
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

#endif /* _NATIVE_CPU_NOVA_H_ */
