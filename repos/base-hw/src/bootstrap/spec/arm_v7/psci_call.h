/**
 * \brief  PSCI call macro for arm_v7
 * \author Piotr Tworek
 * \date   2019-11-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#define PSCI_CALL_IMPL(func_name, instr) \
	static inline int func_name(unsigned int func, unsigned long a0, \
	                            unsigned int a1, unsigned int a2) { \
		unsigned long result = 0; \
		asm volatile ("mov r0, %1 \n" \
		              "mov r1, %2 \n" \
		              "mov r2, %3 \n" \
		              "mov r3, %4 \n" \
		              #instr " #0 \n" \
		              "mov %0, r0 \n" \
		               : "=r"(result) \
		               : "r"(func), "r"(a0), "r"(a1), "r"(a2) \
		               : "r0", "r1", "r2", "r3"); \
		return result; \
	}
