/*
 * \brief  CPU state
 * \author Christian Prochaska
 * \date   2011-04-15
 *
 * This file contains the x86_32-specific part of the CPU state.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__X86_32__CPU__CPU_STATE_H_
#define _INCLUDE__X86_32__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {

	struct Cpu_state
	{
		addr_t ip;   /* instruction pointer */
		addr_t sp;   /* stack pointer       */
		addr_t edi;
		addr_t esi;
		addr_t ebp;
		addr_t ebx;
		addr_t edx;
		addr_t ecx;
		addr_t eax;
		addr_t gs;
		addr_t fs;
		addr_t eflags;
		addr_t trapno;

		/**
		 * Constructor
		 */
		Cpu_state(): ip(0), sp(0), edi(0), esi(0), ebp(0),
		             ebx(0), edx(0), ecx(0), eax(0), gs(0),
		             fs(0), eflags(0), trapno(0) { }
	};
}

#endif /* _INCLUDE__X86_32__CPU__CPU_STATE_H_ */
