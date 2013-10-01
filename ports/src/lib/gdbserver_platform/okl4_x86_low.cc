/*
 * \brief  OKL4(x86)-specific helper functions for GDB server
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
#include "gdb_stub_thread.h"
#include "gdbserver_platform_helper.h"

using namespace Genode;

extern "C" int genode_fetch_register(int regno, unsigned long *reg_content)
{
	Thread_state thread_state;

	try { thread_state = get_current_thread_state(); }
	catch (...) { return 0; }

	switch((enum reg_index)regno)
	{
		case EAX: *reg_content = thread_state.eax; PDBG("EAX = %8lx", *reg_content); return 0;
		case ECX: *reg_content = thread_state.ecx; PDBG("ECX = %8lx", *reg_content); return 0;
		case EDX: *reg_content = thread_state.edx; PDBG("EDX = %8lx", *reg_content); return 0;
		case EBX: *reg_content = thread_state.ebx; PDBG("EBX = %8lx", *reg_content); return 0;
		case UESP: *reg_content = thread_state.sp; PDBG("ESP = %8lx", *reg_content); return 0;
		case EBP:
			/*
			 * When in a syscall, the user EBP has been pushed onto the stack at address ESP+4
			 *
			 * looking for syscall pattern:
			 * EIP-2:  0f 34  sysenter
			 */
			if ((genode_read_memory_byte((void*)(thread_state.ip - 1)) == 0x34) &&
				(genode_read_memory_byte((void*)(thread_state.ip - 2)) == 0x0f)) {
				*reg_content = genode_read_memory_byte((void*)(thread_state.sp + 4)) +
				               (genode_read_memory_byte((void*)(thread_state.sp + 5)) << 8) +
				               (genode_read_memory_byte((void*)(thread_state.sp + 6)) << 16) +
				               (genode_read_memory_byte((void*)(thread_state.sp + 7)) << 24);
				PDBG("EBP = %8lx", *reg_content);
				return 0;
			} else {
				*reg_content = thread_state.ebp;
				PDBG("EBP = %8lx", *reg_content);
				return 0;
			}
		case ESI: *reg_content = thread_state.esi; PDBG("ESI = %8lx", *reg_content); return 0;
		case EDI: *reg_content = thread_state.edi; PDBG("EDI = %8lx", *reg_content); return 0;
		case EIP: *reg_content = thread_state.ip; PDBG("EIP = %8lx", *reg_content); return 0;
		case EFL: *reg_content = thread_state.eflags; PDBG("EFLAGS = %8lx", *reg_content); return 0;
		case CS: PDBG("cannot determine contents of register CS"); return -1;
		case SS: PDBG("cannot determine contents of register SS"); return -1;
		case DS: PDBG("cannot determine contents of register DS"); return -1;
		case ES: PDBG("cannot determine contents of register ES"); return -1;
		case FS: PDBG("cannot determine contents of register FS"); return -1;
		case GS: PDBG("cannot determine contents of register GS"); return -1;
	}

	return -1;
}


extern "C" void genode_store_register(int regno, unsigned long reg_content)
{
	PDBG("not implemented yet for this platform");
}

