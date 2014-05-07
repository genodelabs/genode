/*
 * \brief  CPU state
 * \author Christian Prochaska
 * \author Stefan Kalkowski
 * \date   2011-04-15
 *
 * This file contains the x86_64-specific part of the CPU state.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__X86_64__CPU__CPU_STATE_H_
#define _INCLUDE__X86_64__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {

	struct Cpu_state
	{
		addr_t ip;
		addr_t sp;
		addr_t r8;
		addr_t r9;
		addr_t r10;
		addr_t r11;
		addr_t r12;
		addr_t r13;
		addr_t r14;
		addr_t r15;
		addr_t rax;
		addr_t rbx;
		addr_t rcx;
		addr_t rdx;
		addr_t rdi;
		addr_t rsi;
		addr_t rbp;
		addr_t ss;
		addr_t eflags;
		addr_t trapno;

		Cpu_state() : ip(0), sp(0), r8(0), r9(0), r10(0),
		              r11(0), r12(0), r13(0), r14(0), r15(0),
		              rax(0), rbx(0), rcx(0), rdx(0), rdi(0),
		              rsi(0), rbp(0), ss(0), eflags(0), trapno(0) {}
	};
}

#endif /* _INCLUDE__X86_64__CPU__CPU_STATE_H_ */
