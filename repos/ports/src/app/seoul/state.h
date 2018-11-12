/*
 * \brief  Transform state between Genode VM session interface and Seoul
 * \author Alexander Boettcher
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STATE_H_
#define _STATE_H_

#include <cpu/vm_state.h>

#include <nul/vcpu.h>

namespace Seoul {
	void write_vm_state(CpuState &, unsigned mtr, Genode::Vm_state &);
	unsigned read_vm_state(Genode::Vm_state &, CpuState &);
}

#endif /* _STATE_H_ */
