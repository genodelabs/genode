/*
 * \brief   Virtual machine state
 * \author  Benjamin Lamowski
 * \date    2022-10-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PC__VM_STATE_H_
#define _INCLUDE__SPEC__PC__VM_STATE_H_

/* x86 CPU state */
#include <cpu/vcpu_state.h>
#include <virtualization/svm.h>

namespace Genode {

	/**
	 * CPU context of a virtual machine
	 */
	struct Vm_data;

	struct Vm_state;
}

struct Genode::Vm_state : Genode::Vcpu_state
{
};

struct Genode::Vm_data
{
	Board::Vmcb        vmcb;
	Genode::addr_t     vmcb_phys_addr;
	Genode::Vm_state * vm_state;
};

#endif /* _INCLUDE__SPEC__PC__VM_STATE_H_ */
