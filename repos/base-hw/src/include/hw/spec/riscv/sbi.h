/**
 * \brief  OpenSBI interface
 * \author Sebastian Sumpf
 * \date   2021-01-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__RISCV__SBI_H_
#define _SRC__LIB__HW__SPEC__RISCV__SBI_H_

namespace Sbi {

	enum Eid {
		SET_TIMER = 0,
		PUT_CHAR  = 1,
	};

	inline unsigned long ecall(Eid eid, unsigned long arg0)
	{
		asm volatile ("mv a0, %0\n"
		              "mv a7, %1\n"
		              "ecall    \n"
		              "mv %0, a0\n"
		              :  "+r"(arg0) : "r"(eid)
		              : "a0", "a7");
		return eid;
	}

	inline void set_timer(unsigned long value)    { ecall(SET_TIMER, value); }
	inline void console_put_char(unsigned long c) { ecall(PUT_CHAR, c); }
}

namespace Hw { struct Riscv_uart; }

/**
 * SBI Uart
 */
struct Hw::Riscv_uart
{
	void put_char(char const c)
	{
		Sbi::console_put_char(c);
	}
};

#endif /* _SRC__LIB__HW__SPEC__RISCV__SBI_H_ */
