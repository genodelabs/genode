/*
 * \brief   Backtrace helper utility (arm_v8a)
 * \author  Stefan Kalkowski
 * \author  Christian Helmuth
 * \date    2020-01-21
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__OS__FOR_EACH_RETURN_ADDRESS_H_
#define _INCLUDE__SPEC__ARM_64__OS__FOR_EACH_RETURN_ADDRESS_H_

/* included from os/backtrace.h */

void Genode::for_each_return_address(Const_byte_range_ptr const &stack, auto const &fn)
{
	void **fp;

	asm volatile ("mov %0, x29" : "=r"(fp) ::);

	while (stack.contains(fp) && stack.contains(fp + 1) && fp[1]) {
		fn(fp + 1);
		fp = (void **) fp[0];
	}
}

#endif /* _INCLUDE__SPEC__ARM_64__OS__FOR_EACH_RETURN_ADDRESS_H_ */
