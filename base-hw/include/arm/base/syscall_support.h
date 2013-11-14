/*
 * \brief  Syscall declarations specific for ARM systems
 * \author Martin Stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM__BASE__SYSCALL_H_
#define _INCLUDE__ARM__BASE__SYSCALL_H_

/* Genode includes */
#include <base/stdint.h>

namespace Kernel
{
	typedef Genode::uint32_t Syscall_arg;
	typedef Genode::uint32_t Syscall_ret;

	/**
	 * Registers that are provided by a kernel thread-object for user access
	 */
	struct Thread_reg_id
	{
		enum {
			R0            = 0,
			R1            = 1,
			R2            = 2,
			R3            = 3,
			R4            = 4,
			R5            = 5,
			R6            = 6,
			R7            = 7,
			R8            = 8,
			R9            = 9,
			R10           = 10,
			R11           = 11,
			R12           = 12,
			SP            = 13,
			LR            = 14,
			IP            = 15,
			CPSR          = 16,
			CPU_EXCEPTION = 17,
			FAULT_TLB     = 18,
			FAULT_ADDR    = 19,
			FAULT_WRITES  = 20,
			FAULT_SIGNAL  = 21,
		};
	};

	/**
	 * Events that are provided by a kernel thread-object for user handling
	 */
	struct Thread_event_id
	{
		enum { FAULT = 0 };
	};
}

#endif /* _INCLUDE__ARM__BASE__SYSCALL_H_ */

