/*
 * \brief  CPU state
 * \author Christian Prochaska
 * \date   2011-04-15
 *
 * This file contains the x86_32-specific part of the CPU state.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_32__CPU__CPU_STATE_H_
#define _INCLUDE__SPEC__X86_32__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode { struct Cpu_state; }


struct Genode::Cpu_state
{
	addr_t ip     = 0;   /* instruction pointer */
	addr_t sp     = 0;   /* stack pointer       */
	addr_t edi    = 0;
	addr_t esi    = 0;
	addr_t ebp    = 0;
	addr_t ebx    = 0;
	addr_t edx    = 0;
	addr_t ecx    = 0;
	addr_t eax    = 0;
	addr_t gs     = 0;
	addr_t fs     = 0;
	addr_t eflags = 0;
	addr_t trapno = 0;
};

#endif /* _INCLUDE__SPEC__X86_32__CPU__CPU_STATE_H_ */
