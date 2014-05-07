/*
 * \brief  Linux(x86_32)-specific helper functions for GDB server
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

using namespace Genode;

extern "C" int genode_fetch_register(int regno, unsigned long *reg_content)
{
	Thread_state thread_state;

	try { thread_state = get_current_thread_state(); }
	catch (...) { return 0; }

	switch((enum reg_index)regno)
	{
		case EAX: PDBG("cannot determine contents of register EAX"); return -1;
		case ECX: PDBG("cannot determine contents of register ECX"); return -1;
		case EDX: PDBG("cannot determine contents of register EDX"); return -1;
		case EBX: PDBG("cannot determine contents of register EBX"); return -1;
		case UESP: *reg_content = thread_state.sp; PDBG("ESP = %8lx", *reg_content); return 0;
		case EBP: PDBG("cannot determine contents of register EBP"); return -1;
		case ESI: PDBG("cannot determine contents of register ESI"); return -1;
		case EDI: PDBG("cannot determine contents of register EDI"); return -1;
		case EIP: *reg_content = thread_state.ip; PDBG("EIP = %8lx", *reg_content); return 0;
		case EFL: PDBG("cannot determine contents of register EFLAGS"); return -1;
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

