 /*
 * \brief  hw vCPU RPC interface
 * \author Christian Helmuth
 * \date   2021-01-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__HW_NATIVE_VCPU__HW_NATIVE_VCPU_H_
#define _INCLUDE__HW_NATIVE_VCPU__HW_NATIVE_VCPU_H_

#include <vm_session/vm_session.h>
#include <dataspace/dataspace.h>

struct Genode::Vm_session::Native_vcpu : Interface
{
	GENODE_RPC(Rpc_state, Capability<Dataspace>, state);
	GENODE_RPC(Rpc_native_vcpu, Native_capability, native_vcpu);
	GENODE_RPC(Rpc_exception_handler, void, exception_handler, Signal_context_capability);

	GENODE_RPC_INTERFACE(Rpc_state, Rpc_native_vcpu, Rpc_exception_handler);
};

#endif /* _INCLUDE__HW_NATIVE_VCPU__HW_NATIVE_VCPU_H_ */
