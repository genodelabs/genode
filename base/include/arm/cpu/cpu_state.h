/*
 * \brief  CPU state
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM__CPU__CPU_STATE_H_
#define _INCLUDE__ARM__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {

	struct Cpu_state
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
			MAX_CPU_EXCEPTION      = FAST_INTERRUPT_REQUEST,
		};

		enum { MAX_GPR = 13 };

		addr_t r[MAX_GPR]; /* r0-r12 - general purpose        */
		addr_t sp;         /* r13 - stack pointer             */
		addr_t lr;         /* r14 - link register             */
		addr_t ip;         /* r15 - instruction pointer       */
		addr_t cpsr;       /* current program status register */
		Cpu_exception cpu_exception;   /* last exception */
	};


	struct Cpu_state_modes : Cpu_state
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

			uint32_t sp;   /* banked stack pointer */
			uint32_t lr;   /* banked link register */
			uint32_t spsr; /* saved program status register */
		};

		Mode_state mode[Mode_state::MAX]; /* exception mode registers   */
		uint32_t   fiq_r[5];              /* fast-interrupt mode r8-r12 */
	};
}

#endif /* _INCLUDE__ARM__CPU__CPU_STATE_H_ */
