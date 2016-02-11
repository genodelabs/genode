/**
 * \brief  Calls supported by machine mode (or SBI interface in RISC-V)
 * \author Sebastian Sumpf
 * \date   2015-06-14
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MACHINE_CALL_H_
#define _MACHINE_CALL_H_

#include <base/stdint.h>

namespace Machine {

	enum Call {
		PUT_CHAR      = 0x100, /* output character */
		SET_SYS_TIMER = 0x101, /* set timer */
		IS_USER_MODE  = 0x102, /* check if we are in user mode */
	};

	inline void call(Call const number, Genode::addr_t const arg0)
	{
		register Genode::addr_t a0 asm("a0") = number;;
		register Genode::addr_t a1 asm("a1") = arg0;

		asm volatile ("ecall\n" : : "r"(a0), "r"(a1));
	}
}

#endif /* _MACHINE_CALL_H_ */
