/*
 * \brief  CPU state
 * \author Sebastian Sumpf
 * \date   2015-06-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RISCV__CPU__CPU_STATE_H_
#define _INCLUDE__RISCV__CPU__CPU_STATE_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode { struct Cpu_state; }

struct Genode::Cpu_state
{
	enum Cpu_exception {
		INSTRUCTION_UNALIGNED  = 0,
		INSTRUCTION_PAGE_FAULT = 1,
		INSTRUCTION_ILLEGAL    = 2,
		BREAKPOINT             = 3,
		LOAD_UNALIGNED         = 4,
		LOAD_PAGE_FAULT        = 5,
		STORE_UNALIGNED        = 6,
		STORE_PAGE_FAULT       = 7,
		ECALL_FROM_USER        = 8,
		ECALL_FROM_SUPERVISOR  = 9,
		ECALL_FROM_HYPERVISOR  = 10,
		ECALL_FROM_MACHINE     = 11,
		RESET                  = 16,
		IRQ_FLAG               = 1UL << 63,
	};

	addr_t ip, cpu_exception, ra, sp, gp, tp, t0, t1, t2, s0, s1, a0, a1, a2,
	       a3, a4, a5, a6, a7, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, t3,
	       t4, t5, t6;

	bool      is_irq() { return cpu_exception & IRQ_FLAG; }
	unsigned  irq()    { return cpu_exception ^ IRQ_FLAG; }
};

#endif /* _INCLUDE__RISCV__CPU__CPU_STATE_H_ */
