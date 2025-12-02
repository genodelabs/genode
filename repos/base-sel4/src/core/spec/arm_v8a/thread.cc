/*
 * \brief  Utilities for thread creation on seL4
 * \author Alexander Boettcher
 * \date   2025-08-10
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
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


bool Core::start_sel4_thread(Cap_sel tcb_sel, addr_t ip, addr_t sp,
                             unsigned cpu, addr_t const virt_utcb)
{
	/* set register values for the instruction pointer and stack pointer */
	seL4_UserContext regs;
	bzero(&regs, sizeof(regs));
	size_t const num_regs = sizeof(regs)/sizeof(seL4_Word);

	regs.pc = ip;
	regs.sp = sp;

	auto ret = seL4_TCB_WriteRegisters(tcb_sel.value(), false, 0,
	                                   num_regs, &regs);
	if (ret != seL4_NoError)
		return false;

	if (!affinity_sel4_thread(tcb_sel, cpu))
		return false;

	/*
	 * Set tls pointer to location, where ipcbuffer address is stored, so
	 * that it can be used by seL4_GetIPCBuffer()
	 */
	ret = seL4_TCB_SetTLSBase(tcb_sel.value(),
	                          virt_utcb + Native_utcb::tls_ipcbuffer_offset);
	if (ret != seL4_NoError)
		return false;

	return seL4_TCB_Resume(tcb_sel.value()) == seL4_NoError;
}


bool Core::affinity_sel4_thread(Cap_sel const &, unsigned cpu)
{
	if (cpu != 0)
		error("could not set affinity of thread");

	return cpu == 0;
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

	state.cpu.r[0]  = registers.x0;
	state.cpu.r[1]  = registers.x1;
	state.cpu.r[2]  = registers.x2;
	state.cpu.r[3]  = registers.x3;
	state.cpu.r[4]  = registers.x4;
	state.cpu.r[5]  = registers.x5;
	state.cpu.r[6]  = registers.x6;
	state.cpu.r[7]  = registers.x7;
	state.cpu.r[8]  = registers.x8;
	state.cpu.r[9]  = registers.x9;
	state.cpu.r[10] = registers.x10;
	state.cpu.r[11] = registers.x11;
	state.cpu.r[12] = registers.x12;
	state.cpu.r[13] = registers.x13;
	state.cpu.r[14] = registers.x14;
	state.cpu.r[15] = registers.x15;
	state.cpu.r[16] = registers.x16;
	state.cpu.r[17] = registers.x17;
	state.cpu.r[18] = registers.x18;
	state.cpu.r[19] = registers.x19;
	state.cpu.r[20] = registers.x20;
	state.cpu.r[21] = registers.x21;
	state.cpu.r[22] = registers.x22;
	state.cpu.r[23] = registers.x23;
	state.cpu.r[24] = registers.x24;
	state.cpu.r[25] = registers.x25;
	state.cpu.r[26] = registers.x26;
	state.cpu.r[27] = registers.x27;
	state.cpu.r[28] = registers.x28;
	state.cpu.r[29] = registers.x29;
	state.cpu.r[30] = registers.x30;

	state.cpu.sp = registers.sp;
	state.cpu.ip = registers.pc;
	state.cpu.esr_el1 = 0; /* XXX */

	return state;
}
