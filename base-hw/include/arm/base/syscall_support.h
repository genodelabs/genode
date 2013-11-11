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
	 * Thread registers that can be accessed via Access_thread_regs
	 */
	struct Access_thread_regs_id
	{
		enum {
			R0, R1, R2, R3, R4,
			R5, R6, R7, R8, R9,
			R10, R11, R12, SP, LR,
			IP, CPSR, CPU_EXCEPTION
		};
	};
}

#endif /* _INCLUDE__ARM__BASE__SYSCALL_H_ */

