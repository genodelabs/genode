/*
 * \brief  CPU state
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__CPU__CPU_STATE_H_
#define _INCLUDE__SPEC__ARM__CPU__CPU_STATE_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode {

	struct Cpu_state;
	struct Cpu_state_modes;
}


/**
 * Basic CPU state
 */
struct Genode::Cpu_state
{
	/**
	 * Native exception types
	 */
	enum Cpu_exception {
		RESET                  = 1,
		UNDEFINED_INSTRUCTION  = 2,
		SUPERVISOR_CALL        = 3,
		PREFETCH_ABORT         = 4,
		DATA_ABORT             = 5,
		INTERRUPT_REQUEST      = 6,
		FAST_INTERRUPT_REQUEST = 7,
	};

	/**
	 * Registers
	 */
	struct
	{
		/* general purpose register 0..12 */
		addr_t r0   = 0;
		addr_t r1   = 0;
		addr_t r2   = 0;
		addr_t r3   = 0;
		addr_t r4   = 0;
		addr_t r5   = 0;
		addr_t r6   = 0;
		addr_t r7   = 0;
		addr_t r8   = 0;
		addr_t r9   = 0;
		addr_t r10  = 0;
		addr_t r11  = 0;
		addr_t r12  = 0;
		addr_t sp   = 0;          /* stack pointer */
		addr_t lr   = 0;          /* link register */
		addr_t ip   = 0;          /* instruction pointer */
		addr_t cpsr = 0;          /* current program status register */
		addr_t cpu_exception = 0; /* last hardware exception */
	};
};


/**
 * Extend CPU state by banked registers
 */
struct Genode::Cpu_state_modes : Cpu_state
{
	/**
	 * Common banked registers for exception modes
	 */
	struct Mode_state {

		enum Mode {
			UND,   /* Undefined      */
			SVC,   /* Supervisor     */
			ABORT, /* Abort          */
			IRQ,   /* Interrupt      */
			FIQ,   /* Fast Interrupt */
			MAX
		};

		addr_t spsr; /* saved program status register */
		addr_t sp;   /* banked stack pointer */
		addr_t lr;   /* banked link register */
	};

	Mode_state mode[Mode_state::MAX];      /* exception mode registers   */
	addr_t     fiq_r[5] { 0, 0, 0, 0, 0 }; /* fast-interrupt mode r8-r12 */
};

#endif /* _INCLUDE__SPEC__ARM__CPU__CPU_STATE_H_ */
