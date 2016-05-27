/*
 * \brief  NOVA-specific part of the CPU session interface
 * \author Norman Feske
 * \date   2016-04-21
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOVA_NATIVE_CPU__FOC_NATIVE_CPU_H_
#define _INCLUDE__NOVA_NATIVE_CPU__FOC_NATIVE_CPU_H_

#include <base/rpc.h>
#include <cpu_session/cpu_session.h>

namespace Genode { struct Nova_native_cpu; }


struct Genode::Nova_native_cpu : Cpu_session::Native_cpu
{
	virtual Native_capability pager_cap(Thread_capability) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_pager_cap, Native_capability, pager_cap, Thread_capability);
	GENODE_RPC_INTERFACE(Rpc_pager_cap);
};

#endif /* _INCLUDE__NOVA_NATIVE_CPU__FOC_NATIVE_CPU_H_ */
