/*
 * \brief  Fiasco.OC(ARM)-specific helper functions for GDB server
 * \author Christian Prochaska
 * \date   2011-08-01
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" {
#define private _private
#include "genode-low.h"
#define _private private
}

#include <base/printf.h>
#include "cpu_session_component.h"
#include "reg-arm.h"
#include "gdbserver_platform_helper.h"

#include "gdb_stub_thread.h"

#undef PDBG
#define PDBG(...)

using namespace Genode;

static bool in_syscall(Thread_state const &thread_state)
{
	/* looking for syscall pattern:
	 * (PC-8:  e1a0e00f mov lr, pc)
	 *  PC-4:  e3e0f00b mvn pc, #11
	 * (PC:    e1a02004 mov r2, r4)
	 */
	if ((genode_read_memory_byte((void*)(thread_state.ip - 1)) == 0xe3) &&
		(genode_read_memory_byte((void*)(thread_state.ip - 2)) == 0xe0) &&
		(genode_read_memory_byte((void*)(thread_state.ip - 3)) == 0xf0) &&
		(genode_read_memory_byte((void*)(thread_state.ip - 4)) == 0x0b))
		return true;

	return false;
}

extern "C" int genode_fetch_register(int regno, unsigned long *reg_content)
{
	Thread_state thread_state;

	try { thread_state = get_current_thread_state(); }
	catch (...) { return 0; }

	if (in_syscall(thread_state) || thread_state.unresolved_page_fault) {
		switch((enum reg_index)regno)
		{
			case R0: PDBG("cannot determine contents of register R0"); return -1;
			case R1: PDBG("cannot determine contents of register R1"); return -1;
			case R2: PDBG("cannot determine contents of register R2"); return -1;
			case R3: PDBG("cannot determine contents of register R3"); return -1;
			case R4: PDBG("cannot determine contents of register R4"); return -1;
			case R5: PDBG("cannot determine contents of register R5"); return -1;
			case R6: PDBG("cannot determine contents of register R6"); return -1;
			case R7: PDBG("cannot determine contents of register R7"); return -1;
			case R8: PDBG("cannot determine contents of register R8"); return -1;
			case R9: PDBG("cannot determine contents of register R9"); return -1;
			case R10: PDBG("cannot determine contents of register R10"); return -1;
			case R11:
				if (in_syscall(thread_state)) {
					/* R11 can be calculated from SP. The offset can be found in
			 	 	 * the disassembled 'Fiasco::l4_ipc()' function:
			 	 	 *   add	r11, sp, #8 -> r11 = sp + 8
			 	 	 *   sub	sp, sp, #20 -> r11 = (sp + 20) + 8
			 	 	 */
					*reg_content = (thread_state.sp + 20) + 8;
					PDBG("FP = %8lx", *reg_content);
					return 0;
				} else {
					PDBG("cannot determine contents of register R11"); return -1;
				}
			case R12: PDBG("cannot determine contents of register R12"); return -1;
			case SP: *reg_content = thread_state.sp; PDBG("SP = %8lx", *reg_content); return 0;
			case LR: PDBG("cannot determine contents of register LR"); return -1;
			case PC: *reg_content = thread_state.ip; PDBG("PC = %8lx", *reg_content); return 0;
			case F0: PDBG("cannot determine contents of register F0"); return -1;
			case F1: PDBG("cannot determine contents of register F1"); return -1;
			case F2: PDBG("cannot determine contents of register F2"); return -1;
			case F3: PDBG("cannot determine contents of register F3"); return -1;
			case F4: PDBG("cannot determine contents of register F4"); return -1;
			case F5: PDBG("cannot determine contents of register F5"); return -1;
			case F6: PDBG("cannot determine contents of register F6"); return -1;
			case F7: PDBG("cannot determine contents of register F7"); return -1;
			case FPS: PDBG("cannot determine contents of register FPS"); return -1;
			case CPSR: PDBG("cannot determine contents of register CPSR"); return -1;
		}
	} else {
		switch((enum reg_index)regno)
		{
			case R0: *reg_content = thread_state.r0; PDBG("R0 = %8lx", *reg_content); return 0;
			case R1: *reg_content = thread_state.r1; PDBG("R1 = %8lx", *reg_content); return 0;
			case R2: *reg_content = thread_state.r2; PDBG("R2 = %8lx", *reg_content); return 0;
			case R3: *reg_content = thread_state.r3; PDBG("R3 = %8lx", *reg_content); return 0;
			case R4: *reg_content = thread_state.r4; PDBG("R4 = %8lx", *reg_content); return 0;
			case R5: *reg_content = thread_state.r5; PDBG("R5 = %8lx", *reg_content); return 0;
			case R6: *reg_content = thread_state.r6; PDBG("R6 = %8lx", *reg_content); return 0;
			case R7: *reg_content = thread_state.r7; PDBG("R7 = %8lx", *reg_content); return 0;
			case R8: *reg_content = thread_state.r8; PDBG("R8 = %8lx", *reg_content); return 0;
			case R9: *reg_content = thread_state.r9; PDBG("R9 = %8lx", *reg_content); return 0;
			case R10: *reg_content = thread_state.r10; PDBG("R10 = %8lx", *reg_content); return 0;
			case R11: *reg_content = thread_state.r11; PDBG("FP = %8lx", *reg_content); return 0;
			case R12: *reg_content = thread_state.r12; PDBG("R12 = %8lx", *reg_content); return 0;
			case SP: *reg_content = thread_state.sp; PDBG("SP = %8lx", *reg_content); return 0;
			case LR: *reg_content = thread_state.lr; PDBG("LR = %8lx", *reg_content); return 0;
			case PC: *reg_content = thread_state.ip; PDBG("PC = %8lx", *reg_content); return 0;
			case F0: PDBG("cannot determine contents of register F0"); return -1;
			case F1: PDBG("cannot determine contents of register F1"); return -1;
			case F2: PDBG("cannot determine contents of register F2"); return -1;
			case F3: PDBG("cannot determine contents of register F3"); return -1;
			case F4: PDBG("cannot determine contents of register F4"); return -1;
			case F5: PDBG("cannot determine contents of register F5"); return -1;
			case F6: PDBG("cannot determine contents of register F6"); return -1;
			case F7: PDBG("cannot determine contents of register F7"); return -1;
			case FPS: PDBG("cannot determine contents of register FPS"); return -1;
			case CPSR: *reg_content = thread_state.cpsr; PDBG("CPSR = %8lx", *reg_content); return 0;
		}
	}

	return -1;
}


extern "C" void genode_store_register(int regno, unsigned long reg_content)
{
	Thread_state thread_state;

	try { thread_state = get_current_thread_state(); }
	catch (...) { return; }

	if (in_syscall(thread_state)) {
		PDBG("cannot set registers while thread is in syscall");
		return;
	}

	switch((enum reg_index)regno)
	{
		case R0:   thread_state.r0  = reg_content; PDBG("R0 = %8lx", reg_content);  break;
		case R1:   thread_state.r1  = reg_content; PDBG("R1 = %8lx", reg_content);  break;
		case R2:   thread_state.r2  = reg_content; PDBG("R2 = %8lx", reg_content);  break;
		case R3:   thread_state.r3  = reg_content; PDBG("R3 = %8lx", reg_content);  break;
		case R4:   thread_state.r4  = reg_content; PDBG("R4 = %8lx", reg_content);  break;
		case R5:   thread_state.r5  = reg_content; PDBG("R5 = %8lx", reg_content);  break;
		case R6:   thread_state.r6  = reg_content; PDBG("R6 = %8lx", reg_content);  break;
		case R7:   thread_state.r7  = reg_content; PDBG("R7 = %8lx", reg_content);  break;
		case R8:   thread_state.r8  = reg_content; PDBG("R8 = %8lx", reg_content);  break;
		case R9:   thread_state.r9  = reg_content; PDBG("R9 = %8lx", reg_content);  break;
		case R10:  thread_state.r10 = reg_content; PDBG("R10 = %8lx", reg_content); break;
		case R11:  thread_state.r11 = reg_content; PDBG("FP = %8lx", reg_content);  break;
		case R12:  thread_state.r12 = reg_content; PDBG("R12 = %8lx", reg_content); break;
		case SP:   thread_state.sp  = reg_content; PDBG("SP = %8lx", reg_content);  break;
		case LR:   thread_state.lr  = reg_content; PDBG("LR = %8lx", reg_content);  break;
		case PC:   thread_state.ip  = reg_content; PDBG("PC = %8lx", reg_content);  break;
		case F0:   PDBG("cannot set contents of register F0"); break;
		case F1:   PDBG("cannot set contents of register F1"); break;
		case F2:   PDBG("cannot set contents of register F2"); break;
		case F3:   PDBG("cannot set contents of register F3"); break;
		case F4:   PDBG("cannot set contents of register F4"); break;
		case F5:   PDBG("cannot set contents of register F5"); break;
		case F6:   PDBG("cannot set contents of register F6"); break;
		case F7:   PDBG("cannot set contents of register F7"); break;
		case FPS:  PDBG("cannot set contents of register FPS"); break;
		case CPSR: thread_state.cpsr = reg_content; PDBG("CPSR = %8lx", reg_content); break;
	}

	set_current_thread_state(thread_state);
}

