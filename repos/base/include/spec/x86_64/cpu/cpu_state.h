/*
 * \brief  CPU state
 * \author Adrian-Ken Rueegsegger
 * \author Christian Prochaska
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2011-04-15
 *
 * This file contains the x86_64-specific part of the CPU state.
 */

/*
 * Copyright (C) 2011-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__CPU__CPU_STATE_H_
#define _INCLUDE__SPEC__X86_64__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode { struct Cpu_state; }

struct Genode::Cpu_state
{
	enum Cpu_exception {
		DIVIDE_ERROR          = 0x00,
		DEBUG                 = 0x01,
		BREAKPOINT            = 0x03,
		UNDEFINED_INSTRUCTION = 0x06,
		NO_MATH_COPROC        = 0x07,
		GENERAL_PROTECTION    = 0x0d,
		PAGE_FAULT            = 0x0e,
		ALIGNMENT_CHECK       = 0x11,
		SUPERVISOR_CALL       = 0x80,
		INTERRUPTS_START      = 0x20,
		RESET                 = 0xfe,
		INTERRUPTS_END        = 0xff,
	};

	addr_t r8      = 0;
	addr_t r9      = 0;
	addr_t r10     = 0;
	addr_t r11     = 0;
	addr_t r12     = 0;
	addr_t r13     = 0;
	addr_t r14     = 0;
	addr_t r15     = 0;
	addr_t rax     = 0;
	addr_t rbx     = 0;
	addr_t rcx     = 0;
	addr_t rdx     = 0;
	addr_t rdi     = 0;
	addr_t rsi     = 0;
	addr_t rbp     = 0;
	addr_t trapno  = RESET;
	addr_t errcode = 0;
	addr_t ip      = 0;
	addr_t cs      = 0;
	addr_t eflags  = 0;
	addr_t sp      = 0;
	addr_t ss      = 0;

	enum {
		ACPI_SUSPEND_REQUEST = 0x100, /* convention for system_control() */
		MSR_ACCESS           = 0x101, /* convention for system_control() */
	};
};

#endif /* _INCLUDE__SPEC__X86_64__CPU__CPU_STATE_H_ */
