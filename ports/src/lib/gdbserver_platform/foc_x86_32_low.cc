/*
 * \brief  Fiasco.OC(x86_32)-specific helper functions for GDB server
 * \author Christian Prochaska
 * \date   2011-05-06
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
#include "i386.h"
#include "cpu_session_component.h"
#include "gdbserver_platform_helper.h"
#include "gdb_stub_thread.h"

#undef PDBG
#define PDBG(...)

using namespace Genode;

static bool in_syscall(Thread_state const &thread_state)
{
	/* looking for syscall pattern:
	 * EIP-7:  55     push %ebp
	 * EIP-6:  ff 93  call ...
	 * EIP:    5d     pop %ebp
	 */
	if ((genode_read_memory_byte((void*)(thread_state.ip)) == 0x5d) &&
		(genode_read_memory_byte((void*)(thread_state.ip - 5)) == 0x93) &&
		(genode_read_memory_byte((void*)(thread_state.ip - 6)) == 0xff) &&
		(genode_read_memory_byte((void*)(thread_state.ip - 7)) == 0x55))
		return true;

	return false;
}

extern "C" int genode_fetch_register(int regno, unsigned long *reg_content)
{
	Thread_state thread_state;

	try { thread_state = get_current_thread_state(); }
	catch (...) { return 0; }

	if (in_syscall(thread_state)) {
		switch((enum reg_index)regno)
		{
			case EAX:  PDBG("cannot determine contents of register EAX"); return -1;
			case ECX:  PDBG("cannot determine contents of register ECX"); return -1;
			case EDX:  PDBG("cannot determine contents of register EDX"); return -1;
			case EBX:
				/* When in a syscall, the user EBX has been pushed onto the stack at address ESP+4 */
				*reg_content = genode_read_memory_byte((void*)(thread_state.sp + 4)) +
				               (genode_read_memory_byte((void*)(thread_state.sp + 5)) << 8) +
				               (genode_read_memory_byte((void*)(thread_state.sp + 6)) << 16) +
				               (genode_read_memory_byte((void*)(thread_state.sp + 7)) << 24);
				PDBG("EBX = %8lx", *reg_content);
				return 0;
			case UESP: *reg_content = thread_state.sp; PDBG("ESP = %8lx", *reg_content); return 0;
			case EBP:
				/* When in a syscall, the user EBP has been pushed onto the stack at address ESP+0 */
				*reg_content = genode_read_memory_byte((void*)(thread_state.sp + 0)) +
							   (genode_read_memory_byte((void*)(thread_state.sp + 1)) << 8) +
							   (genode_read_memory_byte((void*)(thread_state.sp + 2)) << 16) +
							   (genode_read_memory_byte((void*)(thread_state.sp + 3)) << 24);
				PDBG("EBP = %8lx", *reg_content);
				return 0;
			case ESI:  PDBG("cannot determine contents of register ESI"); return -1;
			case EDI:  PDBG("cannot determine contents of register EDI"); return -1;
			case EIP:  *reg_content = thread_state.ip; PDBG("EIP = %8lx", *reg_content); return 0;
			case EFL:  PDBG("cannot determine contents of register EFLAGS"); return -1;
			case CS:   PDBG("cannot determine contents of register CS"); return -1;
			case SS:   PDBG("cannot determine contents of register SS"); return -1;
			case DS:   PDBG("cannot determine contents of register DS"); return -1;
			case ES:   PDBG("cannot determine contents of register ES"); return -1;
			case FS:   PDBG("cannot determine contents of register FS"); return -1;
			case GS:   PDBG("cannot determine contents of register GS"); return -1;
		}
	} else {
		switch((enum reg_index)regno)
		{
			case EAX:  *reg_content = thread_state.eax; PDBG("EAX = %8lx", *reg_content); return 0;
			case ECX:  *reg_content = thread_state.ecx; PDBG("ECX = %8lx", *reg_content); return 0;
			case EDX:  *reg_content = thread_state.edx; PDBG("EDX = %8lx", *reg_content); return 0;
			case EBX:  *reg_content = thread_state.ebx; PDBG("EBX = %8lx", *reg_content); return 0;
			case UESP: *reg_content = thread_state.sp; PDBG("ESP = %8lx", *reg_content); return 0;
			case EBP:  *reg_content = thread_state.ebp; PDBG("EBP = %8lx", *reg_content); return 0;
			case ESI:  *reg_content = thread_state.esi; PDBG("ESI = %8lx", *reg_content); return 0;
			case EDI:  *reg_content = thread_state.edi; PDBG("EDI = %8lx", *reg_content); return 0;
			case EIP:  *reg_content = thread_state.ip; PDBG("EIP = %8lx", *reg_content); return 0;
			case EFL:  *reg_content = thread_state.eflags; PDBG("EFLAGS = %8lx", *reg_content); return 0;
			case CS:   PDBG("cannot determine contents of register CS"); return -1;
			case SS:   PDBG("cannot determine contents of register SS"); return -1;
			case DS:   PDBG("cannot determine contents of register DS"); return -1;
			case ES:   PDBG("cannot determine contents of register ES"); return -1;
			case FS:   *reg_content = thread_state.fs; PDBG("FS = %8lx", *reg_content); return 0;
			case GS:   *reg_content = thread_state.gs; PDBG("GS = %8lx", *reg_content); return 0;
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
		case EAX:  thread_state.eax    = reg_content; PDBG("EAX = %8lx", reg_content); break;
		case ECX:  thread_state.ecx    = reg_content; PDBG("ECX = %8lx", reg_content); break;
		case EDX:  thread_state.edx    = reg_content; PDBG("EDX = %8lx", reg_content); break;
		case EBX:  thread_state.ebx    = reg_content; PDBG("EBX = %8lx", reg_content); break;
		case UESP: thread_state.sp     = reg_content; PDBG("ESP = %8lx", reg_content); break;
		case EBP:  thread_state.ebp    = reg_content; PDBG("EBP = %8lx", reg_content); break;
		case ESI:  thread_state.esi    = reg_content; PDBG("ESI = %8lx", reg_content); break;
		case EDI:  thread_state.edi    = reg_content; PDBG("EDI = %8lx", reg_content); break;
		case EIP:  thread_state.ip     = reg_content; PDBG("EIP = %8lx", reg_content); break;
		case EFL:  thread_state.eflags = reg_content; PDBG("EFL = %8lx", reg_content); break;
		case CS:   PDBG("cannot set contents of register CS"); PDBG(" CS = %8lx", reg_content); break;
		case SS:   PDBG("cannot set contents of register SS"); PDBG(" SS = %8lx", reg_content); break;
		case DS:   PDBG("cannot set contents of register DS"); PDBG(" DS = %8lx", reg_content); break;
		case ES:   PDBG("cannot set contents of register ES"); PDBG(" ES = %8lx", reg_content); break;
		case FS:   thread_state.fs     = reg_content; PDBG(" FS = %8lx", reg_content); break;
		case GS:   thread_state.gs     = reg_content; PDBG(" GS = %8lx", reg_content); break;

	}

	set_current_thread_state(thread_state);
}

