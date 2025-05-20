/*
 * \brief   Backtrace helper utility (riscv)
 * \author  Stefan Kalkowski
 * \author  Christian Helmuth
 * \author  Sebastian Sumpf
 * \date    2025-05-20
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__RISCV__OS__FOR_EACH_RETURN_ADDRESS_H_
#define _INCLUDE__SPEC__RISCV__OS__FOR_EACH_RETURN_ADDRESS_H_

/* included from os/backtrace.h */

void Genode::for_each_return_address(Const_byte_range_ptr const &stack, auto const &fn)
{
	void **fp;

	asm volatile ("mv %0, fp" : "=r"(fp) ::);

	while (stack.contains(fp - 1) && stack.contains(fp - 2) && *(fp - 2)) {
		fn(fp - 1);
		fp = (void **) *(fp - 2);
	}
}

#endif /* _INCLUDE__SPEC__RISCV__OS__FOR_EACH_RETURN_ADDRESS_H_ */
