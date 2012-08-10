/*
 * \brief  NOVA specific helper functions for GDB server
 * \author Alexander Boettcher
 * \date   2012-08-09
 */

/*
 * Copyright (C) 2012-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "i386.h"
#include "cpu_session_component.h"
#include "gdbserver_platform_helper.h"
#include "gdb_stub_thread.h"

using namespace Genode;

extern "C" int genode_fetch_register(int regno, unsigned long *reg_content)
{
	Thread_state thread_state;

	if (!get_current_thread_state(thread_state))
		return -1;

	switch((enum reg_index)regno)
	{
		case EAX:  *reg_content = thread_state.eax; return 0;
		case ECX:  *reg_content = thread_state.ecx; return 0;
		case EDX:  *reg_content = thread_state.edx; return 0;
		case EBX:  *reg_content = thread_state.ebx; return 0;
		case UESP: *reg_content = thread_state.sp;  return 0;
		case EBP:  *reg_content = thread_state.ebp; return 0;
		case ESI:  *reg_content = thread_state.esi; return 0;
		case EDI:  *reg_content = thread_state.edi; return 0;
		case EIP:  *reg_content = thread_state.ip;  return 0;
		case EFL:  *reg_content = thread_state.eflags; return 0;
		case CS:   return -1;
		case SS:   return -1;
		case DS:   return -1;
		case ES:   return -1;
		case FS:   *reg_content = thread_state.fs; return 0;
		case GS:   *reg_content = thread_state.gs; return 0;
	}

	return -1;
}
