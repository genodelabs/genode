/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/pd.h>
#include <kernel/thread.h>

using namespace Kernel;

void Kernel::Thread::_init() { }

void Thread::exception(unsigned const cpu)
{
	if (is_irq())
		return;

	switch(cpu_exception) {
	case ECALL_FROM_USER:
		_call();
		ip += 4; /* set to next instruction */
		break;
	case INSTRUCTION_PAGE_FAULT:
	case STORE_PAGE_FAULT:
	case LOAD_PAGE_FAULT:
		_mmu_exception();
		break;
	default:
		Genode::warning(*this, ": unhandled exception ", cpu_exception,
		                " at ip=", (void*)ip, " addr=", Cpu::sbadaddr());
		_die();
	}
}


void Thread::_mmu_exception()
{
	_become_inactive(AWAITS_RESTART);
	_fault_pd     = (addr_t)_pd->platform_pd();
	_fault_signal = (addr_t)_fault.signal_context();
	_fault_addr   = Cpu::sbadaddr();

	_fault.submit();
}


void Thread::_call_update_pd()
{
	Cpu::sfence();
}


void Thread::_call_update_data_region()
{
	Cpu::sfence();
}


void Thread::_call_update_instr_region() { }


void Thread_event::_signal_acknowledged()
{
	_thread->_restart();
}
