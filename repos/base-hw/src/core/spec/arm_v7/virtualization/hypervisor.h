/*
 * \brief  Interface between kernel and hypervisor
 * \author Stefan Kalkowski
 * \date   2022-06-13
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SPEC__ARM_V7__VIRTUALIZATION_HYPERVISOR_H_
#define _SPEC__ARM_V7__VIRTUALIZATION_HYPERVISOR_H_

#include <base/stdint.h>
#include <cpu/vm_state_virtualization.h>

namespace Hypervisor {

	struct Host_context;

	enum Call_number {
		WORLD_SWITCH   = 0,
		TLB_INVALIDATE = 1,
	};

	using Call_arg = Genode::umword_t;
	using Call_ret = Genode::umword_t;

	extern "C"
	Call_ret hypervisor_call(Call_arg call_id,
	                         Call_arg arg0,
	                         Call_arg arg1);


	inline void invalidate_tlb(Genode::uint64_t vttbr)
	{
		hypervisor_call(TLB_INVALIDATE,
		                (vttbr & 0xffffffff),
		                ((vttbr >> 32U) & 0xffffffff));
	}


	inline void switch_world(Genode::Vm_state & vm_state,
	                         Host_context     & host_state)
	{
		hypervisor_call(WORLD_SWITCH,
		                (Call_arg)&vm_state,
		                (Call_arg)&host_state);
	}
}

#endif /* _SPEC__ARM_V7__VIRTUALIZATION_HYPERVISOR_H_ */
