 /*
 * \brief  Fiasco.OC vCPU RPC interface
 * \author Christian Helmuth
 * \author Alexander Böttcher
 * \date   2021-01-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC_NATIVE_VCPU__FOC_NATIVE_VCPU_H_
#define _INCLUDE__FOC_NATIVE_VCPU__FOC_NATIVE_VCPU_H_

#include <vm_session/vm_session.h>
#include <dataspace/dataspace.h>

#include <foc/syscall.h>

struct Genode::Vm_session::Native_vcpu : Interface
{
	GENODE_RPC(Rpc_foc_vcpu_state, addr_t, foc_vcpu_state);
	GENODE_RPC(Rpc_task_index, Foc::l4_cap_idx_t, task_index);

	GENODE_RPC_INTERFACE(Rpc_task_index, Rpc_foc_vcpu_state);
};

#endif /* _INCLUDE__FOC_NATIVE_VCPU__FOC_NATIVE_VCPU_H_ */
