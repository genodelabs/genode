/*
 * \brief  NOVA (x86_64) specific helper functions for GDB server
 * \author Alexander Boettcher
 * \author Christian Prochaska
 * \date   2014-01-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "amd64.h"
#include "cpu_session_component.h"
#include "gdbserver_platform_helper.h"
#include "genode_child_resources.h"

using namespace Genode;

int genode_fetch_register(int regno, unsigned long *value)
{
	Thread_state ts;

	try { ts = get_current_thread_state(); }
	catch (...) {
		error(__PRETTY_FUNCTION__, ": could not get current thread state");
		return -1;
	}

	switch((enum reg_index)regno)
	{
		case RAX:    fetch_register("RAX", ts.rax,    *value); return 0;
		case RBX:    fetch_register("RBX", ts.rbx,    *value); return 0;
		case RCX:    fetch_register("RCX", ts.rcx,    *value); return 0;
		case RDX:    fetch_register("RDX", ts.rdx,    *value); return 0;
		case RSI:    fetch_register("RSI", ts.rsi,    *value); return 0;
		case RDI:    fetch_register("RDI", ts.rdi,    *value); return 0;
		case RBP:    fetch_register("RBP", ts.rbp,    *value); return 0;
		case RSP:    fetch_register("RSP", ts.sp,     *value); return 0;
		case R8:     fetch_register("R8 ", ts.r8,     *value); return 0;
		case R9:     fetch_register("R9 ", ts.r9,     *value); return 0;
		case R10:    fetch_register("R10", ts.r10,    *value); return 0;
		case R11:    fetch_register("R11", ts.r11,    *value); return 0;
		case R12:    fetch_register("R12", ts.r12,    *value); return 0;
		case R13:    fetch_register("R13", ts.r13,    *value); return 0;
		case R14:    fetch_register("R14", ts.r14,    *value); return 0;
		case R15:    fetch_register("R15", ts.r15,    *value); return 0;
		case RIP:    fetch_register("RIP", ts.ip,     *value); return 0;
		case EFLAGS: fetch_register("RFL", ts.eflags, *value); return 0;
		case CS:     cannot_fetch_register("CS");    return -1;
		case SS:     cannot_fetch_register("SS");    return -1;
		case DS:     cannot_fetch_register("DS");    return -1;
		case ES:     cannot_fetch_register("ES");    return -1;
		case FS:     cannot_fetch_register("FS");    return -1;
		case GS:     cannot_fetch_register("GS");    return -1;
		case ST0:    cannot_fetch_register("ST0");   return -1;
		case ST1:    cannot_fetch_register("ST1");   return -1;
		case ST2:    cannot_fetch_register("ST2");   return -1;
		case ST3:    cannot_fetch_register("ST3");   return -1;
		case ST4:    cannot_fetch_register("ST4");   return -1;
		case ST5:    cannot_fetch_register("ST5");   return -1;
		case ST6:    cannot_fetch_register("ST6");   return -1;
		case ST7:    cannot_fetch_register("ST7");   return -1;
		case FCTRL:  cannot_fetch_register("FCTRL"); return -1;
		case FSTAT:  cannot_fetch_register("FSTAT"); return -1;
		case FTAG:   cannot_fetch_register("FTAG");  return -1;
		case FISEG:  cannot_fetch_register("FISEG"); return -1;
		case FIOFF:  cannot_fetch_register("FIOFF"); return -1;
		case FOSEG:  cannot_fetch_register("FOSEG"); return -1;
		case FOOFF:  cannot_fetch_register("FOOFF"); return -1;
		case FOP:    cannot_fetch_register("FOP");   return -1;
		case XMM0:   cannot_fetch_register("XMM0");  return -1;
		case XMM1:   cannot_fetch_register("XMM1");  return -1;
		case XMM2:   cannot_fetch_register("XMM2");  return -1;
		case XMM3:   cannot_fetch_register("XMM3");  return -1;
		case XMM4:   cannot_fetch_register("XMM4");  return -1;
		case XMM5:   cannot_fetch_register("XMM5");  return -1;
		case XMM6:   cannot_fetch_register("XMM6");  return -1;
		case XMM7:   cannot_fetch_register("XMM7");  return -1;
		case XMM8:   cannot_fetch_register("XMM8");  return -1;
		case XMM9:   cannot_fetch_register("XMM9");  return -1;
		case XMM10:  cannot_fetch_register("XMM10"); return -1;
		case XMM11:  cannot_fetch_register("XMM11"); return -1;
		case XMM12:  cannot_fetch_register("XMM12"); return -1;
		case XMM13:  cannot_fetch_register("XMM13"); return -1;
		case XMM14:  cannot_fetch_register("XMM14"); return -1;
		case XMM15:  cannot_fetch_register("XMM15"); return -1;
		case MXCSR:  cannot_fetch_register("MXCSR"); return -1;
		default:     error("unhandled register ", regno); return -1;
	}

	return -1;
}


void genode_store_register(int regno, unsigned long value)
{
	Thread_state ts;

	try { ts = get_current_thread_state(); }
	catch (...) {
		error(__PRETTY_FUNCTION__, ": could not get current thread state");
		return;
	}

	switch ((enum reg_index)regno)
	{
		case RAX:    if (!store_register("RAX", ts.rax,    value)) return; break;
		case RBX:    if (!store_register("RBX", ts.rbx,    value)) return; break;
		case RCX:    if (!store_register("RCX", ts.rcx,    value)) return; break;
		case RDX:    if (!store_register("RDX", ts.rdx,    value)) return; break;
		case RSI:    if (!store_register("RSI", ts.rsi,    value)) return; break;
		case RDI:    if (!store_register("RDI", ts.rdi,    value)) return; break;
		case RBP:    if (!store_register("RBP", ts.rbp,    value)) return; break;
		case RSP:    if (!store_register("RSP", ts.sp,     value)) return; break;
		case R8:     if (!store_register("R8 ", ts.r8,     value)) return; break;
		case R9:     if (!store_register("R9 ", ts.r9,     value)) return; break;
		case R10:    if (!store_register("R10", ts.r10,    value)) return; break;
		case R11:    if (!store_register("R11", ts.r11,    value)) return; break;
		case R12:    if (!store_register("R12", ts.r12,    value)) return; break;
		case R13:    if (!store_register("R13", ts.r13,    value)) return; break;
		case R14:    if (!store_register("R14", ts.r14,    value)) return; break;
		case R15:    if (!store_register("R15", ts.r15,    value)) return; break;
		case RIP:    if (!store_register("RIP", ts.ip,     value)) return; break;
		case EFLAGS: if (!store_register("RFL", ts.eflags, value)) return; break;
		case CS:     cannot_store_register("CS",    value); return;
		case SS:     cannot_store_register("SS",    value); return;
		case DS:     cannot_store_register("DS",    value); return;
		case ES:     cannot_store_register("ES",    value); return;
		case FS:     cannot_store_register("FS",    value); return;
		case GS:     cannot_store_register("GS",    value); return;
		case ST0:    cannot_store_register("ST0",   value); return;
		case ST1:    cannot_store_register("ST1",   value); return;
		case ST2:    cannot_store_register("ST2",   value); return;
		case ST3:    cannot_store_register("ST3",   value); return;
		case ST4:    cannot_store_register("ST4",   value); return;
		case ST5:    cannot_store_register("ST5",   value); return;
		case ST6:    cannot_store_register("ST6",   value); return;
		case ST7:    cannot_store_register("ST7",   value); return;
		case FCTRL:  cannot_store_register("FCTRL", value); return;
		case FSTAT:  cannot_store_register("FSTAT", value); return;
		case FTAG:   cannot_store_register("FTAG",  value); return;
		case FISEG:  cannot_store_register("FISEG", value); return;
		case FIOFF:  cannot_store_register("FIOFF", value); return;
		case FOSEG:  cannot_store_register("FOSEG", value); return;
		case FOOFF:  cannot_store_register("FOOFF", value); return;
		case FOP:    cannot_store_register("FOP",   value); return;
		case XMM0:   cannot_store_register("XMM0",  value); return;
		case XMM1:   cannot_store_register("XMM1",  value); return;
		case XMM2:   cannot_store_register("XMM2",  value); return;
		case XMM3:   cannot_store_register("XMM3",  value); return;
		case XMM4:   cannot_store_register("XMM4",  value); return;
		case XMM5:   cannot_store_register("XMM5",  value); return;
		case XMM6:   cannot_store_register("XMM6",  value); return;
		case XMM7:   cannot_store_register("XMM7",  value); return;
		case XMM8:   cannot_store_register("XMM8",  value); return;
		case XMM9:   cannot_store_register("XMM9",  value); return;
		case XMM10:  cannot_store_register("XMM10", value); return;
		case XMM11:  cannot_store_register("XMM11", value); return;
		case XMM12:  cannot_store_register("XMM12", value); return;
		case XMM13:  cannot_store_register("XMM13", value); return;
		case XMM14:  cannot_store_register("XMM14", value); return;
		case XMM15:  cannot_store_register("XMM15", value); return;
		case MXCSR:  cannot_store_register("MXCSR", value); return;
		default:     error("unhandled register ", regno); return;
	}

	set_current_thread_state(ts);
}
