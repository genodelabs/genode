/*
 * \brief  Fiasco.OC(x86_32)-specific helper functions for GDB server
 * \author Christian Prochaska
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* GDB monitor includes */
#include "i386.h"
#include "cpu_session_component.h"
#include "gdbserver_platform_helper.h"
#include "genode_child_resources.h"

extern "C" {
#define private _private
#include "genode-low.h"
#define _private private
}

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

extern "C" int genode_fetch_register(int regno, unsigned long *value)
{
	Thread_state ts;

	try { ts = get_current_thread_state(); }
	catch (...) {
		error(__PRETTY_FUNCTION__, ": could not get current thread state");
		return -1;
	}

	if (in_syscall(ts) || ts.unresolved_page_fault) {

		switch((enum reg_index)regno)
		{
			case EAX:  cannot_fetch_register("EAX"); return -1;
			case ECX:  cannot_fetch_register("ECX"); return -1;
			case EDX:  cannot_fetch_register("EDX"); return -1;

			case EBX:

				if (in_syscall(ts)) {

					/* When in a syscall, the user EBX has been pushed onto the stack at address ESP+4 */
					*value = genode_read_memory_byte((void*)(ts.sp + 4))
					       + (genode_read_memory_byte((void*)(ts.sp + 5)) << 8)
					       + (genode_read_memory_byte((void*)(ts.sp + 6)) << 16)
					       + (genode_read_memory_byte((void*)(ts.sp + 7)) << 24);

					/* for the debug output, if enabled */
					fetch_register("EBX", *value, *value);

					return 0;

				} else {

					cannot_fetch_register("EBX");

					return -1;
				}

			case UESP: fetch_register("ESP", ts.sp, *value); return 0;

			case EBP:

				if (in_syscall(ts)) {

					/* When in a syscall, the user EBP has been pushed onto the stack at address ESP+0 */
					*value = genode_read_memory_byte((void*)(ts.sp + 0))
					       + (genode_read_memory_byte((void*)(ts.sp + 1)) << 8)
					       + (genode_read_memory_byte((void*)(ts.sp + 2)) << 16)
					       + (genode_read_memory_byte((void*)(ts.sp + 3)) << 24);

					/* for the debug output, if enabled */
					fetch_register("EBP", *value, *value);

					return 0;

				} else {

					cannot_fetch_register("EBP");

					return -1;
				}

			case ESI:  cannot_fetch_register("ESI"); return -1;
			case EDI:  cannot_fetch_register("EDI"); return -1;
			case EIP:  fetch_register("EIP", ts.ip, *value); return 0;
			case EFL:  cannot_fetch_register("EFL"); return -1;
			case CS:   cannot_fetch_register("CS"); return -1;
			case SS:   cannot_fetch_register("SS"); return -1;
			case DS:   cannot_fetch_register("DS"); return -1;
			case ES:   cannot_fetch_register("ES"); return -1;
			case FS:   cannot_fetch_register("FS"); return -1;
			case GS:   cannot_fetch_register("GS"); return -1;
			default:   error("unhandled register ", regno); return -1;
		}

	} else {

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
			case FS:   fetch_register("FS",  ts.fs,     *value); return 0;
			case GS:   fetch_register("GS",  ts.gs,     *value); return 0;
			default:   error("unhandled register ", regno); return -1;
		}
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

	if (in_syscall(ts)) {
		log("cannot set registers while thread is in syscall");
		return;
	}

	switch((enum reg_index)regno)
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
		case FS:   if (!store_register("FS ", ts.fs,     value)) return; break;
		case GS:   if (!store_register("GS ", ts.gs,     value)) return; break;
		default:   error("unhandled register ", regno); return;
	}

	set_current_thread_state(ts);
}

