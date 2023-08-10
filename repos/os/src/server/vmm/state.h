/*
 * \brief  vCPU state
 * \author Benjamin Lamowski
 * \date   2023-06-22
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__STATE_H_
#define _SRC__SERVER__VMM__STATE_H_

#include <cpu/vm_state_virtualization.h>

using Genode::addr_t;
using Genode::Vm_state;

namespace Genode { struct Vcpu_state : Vm_state { }; };

namespace Vmm {
	struct State : Genode::Vcpu_state
	{
		addr_t reg(addr_t idx) const;
		void reg(addr_t idx, addr_t v);
	};
}

#endif /* _SRC__SERVER__VMM__STATE_H_ */
