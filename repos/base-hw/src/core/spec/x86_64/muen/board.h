/*
 * \brief  x86_64_muen constants
 * \author Adrian-Ken Rueegsegger
 * \date   2015-07-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__MUEN__BOARD_H_
#define _CORE__SPEC__X86_64__MUEN__BOARD_H_

#include <hw/spec/x86_64/pc_board.h>
#include <spec/x86_64/muen/pic.h>
#include <spec/x86_64/muen/timer.h>
#include <cpu/cpu_state.h>

namespace Kernel { class Cpu; }

namespace Board {
	using namespace Hw::Pc_board;

	enum {
		TIMER_BASE_ADDR         = 0xe00010000,
		TIMER_SIZE              = 0x1000,
		TIMER_PREEMPT_BASE_ADDR = 0xe00011000,
		TIMER_PREEMPT_SIZE      = 0x1000,

		VECTOR_REMAP_BASE   = 48,
		TIMER_EVENT_PREEMPT = 30,
		TIMER_EVENT_KERNEL  = 31,
		TIMER_VECTOR_KERNEL = 32,
		TIMER_VECTOR_USER   = 50,
	};

	using Vm_state = Genode::Cpu_state;

	enum { VCPU_MAX = 1 };

	struct Vm_page_table {};
	struct Vm_page_table_array {};

	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
}

#endif /* _CORE__SPEC__X86_64__MUEN__BOARD_H_ */
