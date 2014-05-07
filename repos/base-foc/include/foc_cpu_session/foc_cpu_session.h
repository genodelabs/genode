/*
 * \brief  Cpu session interface extension for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2011-04-04
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FOC_CPU_SESSION__FOC_CPU_SESSION_H_
#define _INCLUDE__FOC_CPU_SESSION__FOC_CPU_SESSION_H_

#include <base/stdint.h>
#include <cpu_session/cpu_session.h>

namespace Genode {

	struct Foc_cpu_session : Cpu_session
	{
		virtual ~Foc_cpu_session() { }

		virtual void enable_vcpu(Thread_capability cap, addr_t vcpu_state) = 0;

		virtual Native_capability native_cap(Thread_capability cap) = 0;

		virtual Native_capability alloc_irq() = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_enable_vcpu, void, enable_vcpu, Thread_capability, addr_t);
		GENODE_RPC(Rpc_native_cap, Native_capability, native_cap, Thread_capability);
		GENODE_RPC(Rpc_alloc_irq, Native_capability, alloc_irq);

		GENODE_RPC_INTERFACE_INHERIT(Cpu_session,
		                             Rpc_enable_vcpu, Rpc_native_cap, Rpc_alloc_irq);
	};
}

#endif /* _INCLUDE__FOC_CPU_SESSION__FOC_CPU_SESSION_H_ */
