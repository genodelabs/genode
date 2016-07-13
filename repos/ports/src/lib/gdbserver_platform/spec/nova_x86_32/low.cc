/*
 * \brief  NOVA (x86_32) specific helper functions for GDB server
 * \author Alexander Boettcher
 * \date   2012-08-09
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "i386.h"
#include "cpu_session_component.h"
#include "gdbserver_platform_helper.h"
#include "genode_child_resources.h"

using namespace Genode;

extern "C" int genode_fetch_register(int regno, unsigned long *value)
{
	Thread_state ts;

	try { ts = get_current_thread_state(); }
	catch (...) {
		error(__PRETTY_FUNCTION__, ": could not get current thread state");
		return -1;
	}

	switch((enum reg_index)regno)
	{
		case EAX:  fetch_register("EAX", ts.eax,    *value); return 0;
		case ECX:  fetch_register("ECX", ts.ecx,    *value); return 0;
		case EDX:  fetch_register("EDX", ts.edx,    *value); return 0;
		case EBX:  fetch_register("EBX", ts.ebx,    *value); return 0;
		case UESP: fetch_register("ESP", ts.sp,     *value); return 0;
		case EBP:  fetch_register("EBP", ts.ebp,    *value); return 0;
		case ESI:  fetch_register("ESI", ts.esi,    *value); return 0;
		case EDI:  fetch_register("EDI", ts.edi,    *value); return 0;
		case EIP:  fetch_register("EIP", ts.ip,     *value); return 0;
		case EFL:  fetch_register("EFL", ts.eflags, *value); return 0;
		case CS:   cannot_fetch_register("CS"); return -1;
		case SS:   cannot_fetch_register("SS"); return -1;
		case DS:   cannot_fetch_register("DS"); return -1;
		case ES:   cannot_fetch_register("ES"); return -1;
		case FS:   cannot_fetch_register("FS"); return -1;
		case GS:   cannot_fetch_register("GS"); return -1;
		default:   error("unhandled register ", regno); return -1;
	}

	return -1;
}


extern "C" void genode_store_register(int regno, unsigned long value)
{
	Thread_state ts;

	try { ts = get_current_thread_state(); }
	catch (...) {
		error(__PRETTY_FUNCTION__, ": could not get current thread state");
		return;
	}

	switch ((enum reg_index)regno)
	{
		case EAX:  if (!store_register("EAX", ts.eax,    value)) return; break;
		case ECX:  if (!store_register("ECX", ts.ecx,    value)) return; break;
		case EDX:  if (!store_register("EDX", ts.edx,    value)) return; break;
		case EBX:  if (!store_register("EBX", ts.ebx,    value)) return; break;
		case UESP: if (!store_register("ESP", ts.sp,     value)) return; break;
		case EBP:  if (!store_register("EBP", ts.ebp,    value)) return; break;
		case ESI:  if (!store_register("ESI", ts.esi,    value)) return; break;
		case EDI:  if (!store_register("EDI", ts.edi,    value)) return; break;
		case EIP:  if (!store_register("EIP", ts.ip,     value)) return; break;
		case EFL:  if (!store_register("EFL", ts.eflags, value)) return; break;
		case CS:   cannot_store_register("CS", value); return;
		case SS:   cannot_store_register("SS", value); return;
		case DS:   cannot_store_register("DS", value); return;
		case ES:   cannot_store_register("ES", value); return;
		case FS:   cannot_store_register("FS", value); return;
		case GS:   cannot_store_register("GS", value); return;
		default:   error("unhandled register ", regno); return;
	}

	set_current_thread_state(ts);
}
