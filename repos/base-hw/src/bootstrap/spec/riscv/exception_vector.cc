/*
 * \brief  Exception vector initialization
 * \author Sebastian Sumpf
 * \date   2015-07-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/cpu.h>

extern int _machine_begin, _machine_end;

extern "C" void setup_riscv_exception_vector()
{
	using namespace Genode;

	/* retrieve exception vector */
	addr_t vector;
	asm volatile ("csrr %0, mtvec\n" : "=r"(vector));

	/* copy  machine mode exception vector */
	memcpy((void *)vector,
	       &_machine_begin, (addr_t)&_machine_end - (addr_t)&_machine_begin);
}
