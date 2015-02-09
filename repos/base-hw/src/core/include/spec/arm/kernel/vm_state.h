/*
 * \brief   CPU context of a virtual machine
 * \author  Stefan Kalkowski
 * \author  Martin Stein
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__VM_STATE_H_
#define _KERNEL__VM_STATE_H_

/* Genode includes */
#include <cpu/cpu_state.h>

namespace Kernel
{
	/**
	 * CPU context of a virtual machine
	 */
	struct Vm_state : Genode::Cpu_state_modes { Genode::addr_t dfar; };
}

#endif /* _KERNEL__VM_STATE_H_ */