/*
 * \brief  Fiasco.OC(ARM)-specific helper functions for GDB server
 * \author Christian Prochaska
 * \date   2011-08-01
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
#include "cpu_session_component.h"
#include "reg-arm.h"
#include "gdbserver_platform_helper.h"

#include "genode_child_resources.h"

extern "C" {
#define private _private
#include "genode-low.h"
#define _private private
}

using namespace Genode;

static bool in_syscall(Thread_state const &ts)
{
	try {
		/* looking for syscall pattern:
		 * (PC-8:  e1a0e00f mov lr, pc)
		 *  PC-4:  e3e0f00b mvn pc, #11
		 * (PC:    e1a02004 mov r2, r4)
		 */
		if ((genode_read_memory_byte((void*)(ts.ip - 1)) == 0xe3) &&
			(genode_read_memory_byte((void*)(ts.ip - 2)) == 0xe0) &&
			(genode_read_memory_byte((void*)(ts.ip - 3)) == 0xf0) &&
			(genode_read_memory_byte((void*)(ts.ip - 4)) == 0x0b))
			return true;
	} catch (No_memory_at_address) {
		return false;
	}
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
			case R0:  cannot_fetch_register("R0");  return -1;
			case R1:  cannot_fetch_register("R1");  return -1;
			case R2:  cannot_fetch_register("R2");  return -1;
			case R3:  cannot_fetch_register("R3");  return -1;
			case R4:  cannot_fetch_register("R4");  return -1;
			case R5:  cannot_fetch_register("R5");  return -1;
			case R6:  cannot_fetch_register("R6");  return -1;
			case R7:  cannot_fetch_register("R7");  return -1;
			case R8:  cannot_fetch_register("R8");  return -1;
			case R9:  cannot_fetch_register("R9");  return -1;
			case R10: cannot_fetch_register("R10"); return -1;

			case R11:

				if (in_syscall(ts)) {
					/* R11 can be calculated from SP. The offset can be found in
			 	 	 * the disassembled 'Fiasco::l4_ipc()' function:
			 	 	 *   add	r11, sp, #8 -> r11 = sp + 8
			 	 	 *   sub	sp, sp, #20 -> r11 = (sp + 20) + 8
			 	 	 */
					*value = (ts.sp + 20) + 8;

				    /* for the debug output, if enabled */
				    fetch_register("R11", *value, *value);

					return 0;
				} else {

				   cannot_fetch_register("R11");

				   return -1;
				}

			case R12:  cannot_fetch_register("R12"); return -1;
			case SP:   fetch_register("SP ", ts.sp, *value); return 0;
			case LR:   cannot_fetch_register("LR"); return -1;
			case PC:   fetch_register("PC ", ts.ip, *value); return 0;
			case F0:   cannot_fetch_register("F0"); return -1;
			case F1:   cannot_fetch_register("F1"); return -1;
			case F2:   cannot_fetch_register("F2"); return -1;
			case F3:   cannot_fetch_register("F3"); return -1;
			case F4:   cannot_fetch_register("F4"); return -1;
			case F5:   cannot_fetch_register("F5"); return -1;
			case F6:   cannot_fetch_register("F6"); return -1;
			case F7:   cannot_fetch_register("F7"); return -1;
			case FPS:  cannot_fetch_register("FPS"); return -1;
			case CPSR: cannot_fetch_register("CPSR"); return -1;
			default:   error("unhandled register ", regno); return -1;
		}

	} else {

		switch((enum reg_index)regno)
		{
			case R0:   fetch_register("R0  ", ts.r0,  *value); return 0;
			case R1:   fetch_register("R1  ", ts.r1,  *value); return 0;
			case R2:   fetch_register("R2  ", ts.r2,  *value); return 0;
			case R3:   fetch_register("R3  ", ts.r3,  *value); return 0;
			case R4:   fetch_register("R4  ", ts.r4,  *value); return 0;
			case R5:   fetch_register("R5  ", ts.r5,  *value); return 0;
			case R6:   fetch_register("R6  ", ts.r6,  *value); return 0;
			case R7:   fetch_register("R7  ", ts.r7,  *value); return 0;
			case R8:   fetch_register("R8  ", ts.r8,  *value); return 0;
			case R9:   fetch_register("R9  ", ts.r9,  *value); return 0;
			case R10:  fetch_register("R10 ", ts.r10, *value); return 0;
			case R11:  fetch_register("R11 ", ts.r11, *value); return 0;
			case R12:  fetch_register("R12 ", ts.r12, *value); return 0;
			case SP:   fetch_register("SP  ", ts.sp,  *value); return 0;
			case LR:   fetch_register("LR  ", ts.lr,  *value); return 0;
			case PC:   fetch_register("PC  ", ts.ip,  *value); return 0;
			case F0:   cannot_fetch_register("F0"); return -1;
			case F1:   cannot_fetch_register("F1"); return -1;
			case F2:   cannot_fetch_register("F2"); return -1;
			case F3:   cannot_fetch_register("F3"); return -1;
			case F4:   cannot_fetch_register("F4"); return -1;
			case F5:   cannot_fetch_register("F5"); return -1;
			case F6:   cannot_fetch_register("F6"); return -1;
			case F7:   cannot_fetch_register("F7"); return -1;
			case FPS:  cannot_fetch_register("FPS"); return -1;
			case CPSR: fetch_register("CPSR", ts.cpsr,  *value); return 0;
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
		case R0:   if (!store_register("R0  ", ts.r0,   value)) return; break;
		case R1:   if (!store_register("R1  ", ts.r1,   value)) return; break;
		case R2:   if (!store_register("R2  ", ts.r2,   value)) return; break;
		case R3:   if (!store_register("R3  ", ts.r3,   value)) return; break;
		case R4:   if (!store_register("R4  ", ts.r4,   value)) return; break;
		case R5:   if (!store_register("R5  ", ts.r5,   value)) return; break;
		case R6:   if (!store_register("R6  ", ts.r6,   value)) return; break;
		case R7:   if (!store_register("R7  ", ts.r7,   value)) return; break;
		case R8:   if (!store_register("R8  ", ts.r8,   value)) return; break;
		case R9:   if (!store_register("R9  ", ts.r9,   value)) return; break;
		case R10:  if (!store_register("R10 ", ts.r10,  value)) return; break;
		case R11:  if (!store_register("R11 ", ts.r11,  value)) return; break;
		case R12:  if (!store_register("R12 ", ts.r12,  value)) return; break;
		case SP:   if (!store_register("SP  ", ts.sp,   value)) return; break;
		case LR:   if (!store_register("LR  ", ts.lr,   value)) return; break;
		case PC:   if (!store_register("PC  ", ts.ip,   value)) return; break;
		case F0:   cannot_store_register("F0 ", value); return;
		case F1:   cannot_store_register("F1 ", value); return;
		case F2:   cannot_store_register("F2 ", value); return;
		case F3:   cannot_store_register("F3 ", value); return;
		case F4:   cannot_store_register("F4 ", value); return;
		case F5:   cannot_store_register("F5 ", value); return;
		case F6:   cannot_store_register("F6 ", value); return;
		case F7:   cannot_store_register("F7 ", value); return;
		case FPS:  cannot_store_register("FPS", value); return;
		case CPSR: if (!store_register("CPSR", ts.cpsr, value)) return; break;
	}

	set_current_thread_state(ts);
}

