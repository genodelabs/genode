/**
 * \brief  PSCI call macro for arm_v8
 * \author Piotr Tworek
 * \date   2019-11-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__PSCI_CALL_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__PSCI_CALL_H_

#include <hw/spec/arm/psci.h>

#define PSCI_CALL_IMPL(instr) \
	static inline int call(unsigned int func, unsigned long a0, \
	                       unsigned int a1, unsigned int a2) { \
		unsigned long result = 0; \
		asm volatile ("mov x0, %1 \n" \
		              "mov x1, %2 \n" \
		              "mov x2, %3 \n" \
		              "mov x3, %4 \n" \
		              #instr " #0 \n" \
		              "mov %0, x0 \n" \
		               : "=r"(result) \
		               : "r"(func), "r"(a0), "r"(a1), "r"(a2) \
		               : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", \
		                 "x8", "x9", "x10", "x11", "x12", "x13", "x14"); \
		return result; \
	}

namespace Hw {
	struct Psci_hvc_functor { PSCI_CALL_IMPL(hvc); };
	struct Psci_smc_functor { PSCI_CALL_IMPL(smc); };
}

#undef PSCI_CALL_IMPL

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__PSCI_CALL_H_ */
