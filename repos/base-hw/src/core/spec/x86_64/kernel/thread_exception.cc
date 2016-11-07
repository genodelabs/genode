/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>

using namespace Kernel;

void Thread::exception(unsigned const cpu)
{
	switch (trapno) {
	case PAGE_FAULT:
		_mmu_exception();
		return;
	case NO_MATH_COPROC:
		if (_cpu->fpu().fault(*this)) { return; }
		Genode::warning(*this, ": FPU error");
		_stop();
		return;
	case UNDEFINED_INSTRUCTION:
		Genode::warning(*this, ": undefined instruction at ip=", (void*)ip);
		_stop();
		return;
	case SUPERVISOR_CALL:
		_call();
		return;
	}
	if (trapno >= INTERRUPTS_START && trapno <= INTERRUPTS_END) {
		_interrupt(cpu);
		return;
	}
	Genode::warning(*this, ": triggered unknown exception ", trapno,
	                " with error code ", errcode, " at ip=%p", (void*)ip);
	_stop();
}
