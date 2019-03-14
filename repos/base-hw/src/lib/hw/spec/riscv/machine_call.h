/**
 * \brief  Calls supported by machine mode (or SBI interface in RISC-V)
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2015-06-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__RISCV__MACHINE_CALL_H_
#define _SRC__LIB__HW__SPEC__RISCV__MACHINE_CALL_H_

using uintptr_t = unsigned long;
#include <bbl/mcall.h>

namespace Hw {

	inline unsigned long ecall(unsigned long id, unsigned long a0,
	                           unsigned long a1)
	{
		asm volatile ("mv a0, %0\n"
		              "mv a1, %1\n"
		              "mv a2, %2\n"
		              "ecall    \n"
		              "mv %0, a0\n"
		              :  "+r"(id) : "r"(a0), "r"(a1)
		              : "a0", "a1", "a2");
		return id;
	}

	inline void put_char(unsigned long c) {
		ecall(MCALL_CONSOLE_PUTCHAR, c, /* unused arg */ 0);
	}

	inline void set_sys_timer(unsigned long t) {
		ecall(MCALL_SET_TIMER, t, /* unused arg */ 0); }

	inline unsigned long get_sys_timer() {
		return ecall(MCALL_GET_TIMER, /* unused args */ 0, 0); }
}

#endif /* _SRC__LIB__HW__SPEC__RISCV__MACHINE_CALL_H_ */
