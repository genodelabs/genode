/*
 * \brief   CPU specific implementations of core
 * \author  Martin Stein
 * \author Stefan Kalkowski
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


/*************************
 ** Kernel::Thread_base **
 *************************/

Thread_base::Thread_base(Thread * const t)
:
	_fault(t),
	_fault_pd(0),
	_fault_addr(0),
	_fault_writes(0),
	_fault_signal(0)
{ }


/*************************
 ** CPU-state utilities **
 *************************/

typedef Thread_reg_id Reg_id;

static addr_t const _cpu_state_regs[] = {
	Reg_id::R0,   Reg_id::R1,   Reg_id::R2,  Reg_id::R3, Reg_id::R4,
	Reg_id::R5,   Reg_id::R6,   Reg_id::R7,  Reg_id::R8, Reg_id::R9,
	Reg_id::R10,  Reg_id::R11,  Reg_id::R12, Reg_id::SP, Reg_id::LR,
	Reg_id::IP,   Reg_id::CPSR, Reg_id::CPU_EXCEPTION };

addr_t const * cpu_state_regs() { return _cpu_state_regs; }


size_t cpu_state_regs_length()
{
	return sizeof(_cpu_state_regs)/sizeof(_cpu_state_regs[0]);
}
