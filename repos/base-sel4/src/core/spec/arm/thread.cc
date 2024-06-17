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
#include <base/internal/native_utcb.h>
#include <base/thread_state.h>

/* core includes */
#include <thread_sel4.h>
#include <platform_thread.h>

using namespace Core;


void Core::start_sel4_thread(Cap_sel tcb_sel, addr_t ip, addr_t sp,
                             unsigned cpu, addr_t const virt_utcb)
{
	/* set register values for the instruction pointer and stack pointer */
	seL4_UserContext regs;
	memset(&regs, 0, sizeof(regs));
	size_t const num_regs = sizeof(regs)/sizeof(seL4_Word);

	regs.pc = ip;
	regs.sp = sp;

	long const ret = seL4_TCB_WriteRegisters(tcb_sel.value(), false, 0,
	                                         num_regs, &regs);
	ASSERT(ret == 0);

	affinity_sel4_thread(tcb_sel, cpu);

	/*
	 * Set tls pointer to location, where ipcbuffer address is stored, so
	 * that it can be used by seL4_GetIPCBuffer()
	 */
	auto error = seL4_TCB_SetTLSBase(tcb_sel.value(),
	                                 virt_utcb + Native_utcb::tls_ipcbuffer_offset);
	ASSERT(not error);

	seL4_TCB_Resume(tcb_sel.value());
}


void Core::affinity_sel4_thread(Cap_sel const &, unsigned cpu)
{
	if (cpu != 0)
		error("could not set affinity of thread");
}


Thread_state Platform_thread::state()
{
	seL4_TCB   const thread         = _info.tcb_sel.value();
	seL4_Bool  const suspend_source = false;
	seL4_Uint8 const arch_flags     = 0;
	seL4_UserContext registers;
	seL4_Word  const register_count = sizeof(registers) / sizeof(registers.pc);

	long const ret = seL4_TCB_ReadRegisters(thread, suspend_source, arch_flags,
	                                        register_count, &registers);
	if (ret != seL4_NoError)
		return { .state = Thread_state::State::UNAVAILABLE, .cpu = { } };

	Thread_state state { };

	state.cpu.r0   = registers.r0;
	state.cpu.r1   = registers.r1;
	state.cpu.r2   = registers.r2;
	state.cpu.r3   = registers.r3;
	state.cpu.r4   = registers.r4;
	state.cpu.r5   = registers.r5;
	state.cpu.r6   = registers.r6;
	state.cpu.r7   = registers.r7;
	state.cpu.r8   = registers.r8;
	state.cpu.r9   = registers.r9;
	state.cpu.r10  = registers.r10;
	state.cpu.r11  = registers.r11;
	state.cpu.r12  = registers.r12;
	state.cpu.sp   = registers.sp;
	state.cpu.lr   = registers.r14;
	state.cpu.ip   = registers.pc;
	state.cpu.cpsr = registers.cpsr;
	state.cpu.cpu_exception = 0; /* XXX detect/track if in exception and report here */

	return state;
}
