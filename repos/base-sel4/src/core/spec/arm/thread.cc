/*
 * \brief  Utilities for thread creation on seL4
 * \author Norman Feske
 * \date   2015-05-12
 *
 * This file is used by both the core-specific implementation of the Thread API
 * and the platform-thread implementation for managing threads outside of core.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/thread_state.h>

/* core includes */
#include <thread_sel4.h>
#include <platform_thread.h>

void Genode::start_sel4_thread(Cap_sel tcb_sel, addr_t ip, addr_t sp,
                               unsigned cpu)
{
	/* set register values for the instruction pointer and stack pointer */
	seL4_UserContext regs;
	Genode::memset(&regs, 0, sizeof(regs));
	size_t const num_regs = sizeof(regs)/sizeof(seL4_Word);

	regs.pc = ip;
	regs.sp = sp;

	long const ret = seL4_TCB_WriteRegisters(tcb_sel.value(), false, 0,
	                                         num_regs, &regs);
	ASSERT(ret == 0);

	affinity_sel4_thread(tcb_sel, cpu);

	seL4_TCB_Resume(tcb_sel.value());
}

void Genode::affinity_sel4_thread(Cap_sel const &tcb_sel, unsigned cpu)
{
	if (cpu != 0)
		error("could not set affinity of thread");
}

Genode::Thread_state Genode::Platform_thread::state()
{
	seL4_TCB   const thread         = _info.tcb_sel.value();
	seL4_Bool  const suspend_source = false;
	seL4_Uint8 const arch_flags     = 0;
	seL4_UserContext registers;
	seL4_Word  const register_count = sizeof(registers) / sizeof(registers.pc);

	long const ret = seL4_TCB_ReadRegisters(thread, suspend_source, arch_flags,
	                                        register_count, &registers);
	if (ret != seL4_NoError) {
		error("reading thread state ", ret);
		throw Cpu_thread::State_access_failed();
	}

	Thread_state state;
	Genode::memset(&state, 0, sizeof(state));

	state.r0   = registers.r0;
	state.r1   = registers.r1;
	state.r2   = registers.r2;
	state.r3   = registers.r3;
	state.r4   = registers.r4;
	state.r5   = registers.r5;
	state.r6   = registers.r6;
	state.r7   = registers.r7;
	state.r8   = registers.r8;
	state.r9   = registers.r9;
	state.r10  = registers.r10;
	state.r11  = registers.r11;
	state.r12  = registers.r12;
	state.sp   = registers.sp;
	state.lr   = registers.r14;
	state.ip   = registers.pc;
	state.cpsr = registers.cpsr;
	state.cpu_exception = 0; /* XXX detect/track if in exception and report here */

	return state;
}
