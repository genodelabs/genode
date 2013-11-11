/*
 * \brief   CPU specific implementations of core
 * \author  Martin Stein
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>

using namespace Kernel;


/********************
 ** Kernel::Thread **
 ********************/

addr_t * Kernel::Thread::_reg(addr_t const id) const
{
	static addr_t * const _regs[] = {
		(addr_t *)&r0,
		(addr_t *)&r1,
		(addr_t *)&r2,
		(addr_t *)&r3,
		(addr_t *)&r4,
		(addr_t *)&r5,
		(addr_t *)&r6,
		(addr_t *)&r7,
		(addr_t *)&r8,
		(addr_t *)&r9,
		(addr_t *)&r10,
		(addr_t *)&r11,
		(addr_t *)&r12,
		(addr_t *)&sp,
		(addr_t *)&lr,
		(addr_t *)&ip,
		(addr_t *)&cpsr,
		(addr_t *)&cpu_exception
	};
	return id < sizeof(_regs)/sizeof(_regs[0]) ? _regs[id] : 0;
}
