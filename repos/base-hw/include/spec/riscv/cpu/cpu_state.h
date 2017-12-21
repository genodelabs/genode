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

	addr_t ip            = 0;
	addr_t cpu_exception = 0;
	addr_t ra            = 0;
	addr_t sp            = 0;
	addr_t gp            = 0;
	addr_t tp            = 0;
	addr_t t0            = 0;
	addr_t t1            = 0;
	addr_t t2            = 0;
	addr_t s0            = 0;
	addr_t s1            = 0;
	addr_t a0            = 0;
	addr_t a1            = 0;
	addr_t a2            = 0;
	addr_t a3            = 0;
	addr_t a4            = 0;
	addr_t a5            = 0;
	addr_t a6            = 0;
	addr_t a7            = 0;
	addr_t s2            = 0;
	addr_t s3            = 0;
	addr_t s4            = 0;
	addr_t s5            = 0;
	addr_t s6            = 0;
	addr_t s7            = 0;
	addr_t s8            = 0;
	addr_t s9            = 0;
	addr_t s10           = 0;
	addr_t s11           = 0;
	addr_t t3            = 0;
	addr_t t4            = 0;
	addr_t t5            = 0;
	addr_t t6            = 0;

	bool      is_irq() { return cpu_exception & IRQ_FLAG; }
	unsigned  irq()    { return cpu_exception ^ IRQ_FLAG; }
};

#endif /* _INCLUDE__RISCV__CPU__CPU_STATE_H_ */
