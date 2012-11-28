/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <base/printf.h>
#include <irq_root.h>
#include <kernel/syscalls.h>
#include <base/sleep.h>
#include <xilinx/xps_timer.h>

using namespace Genode;

void Irq_session_component::wait_for_irq()
{
	using namespace Xilinx;
	if (!_attached) {
		if(Kernel::irq_allocate(_irq_number)) {
			PERR("Kernel::irq_allocate(%i) failed", _irq_number);
			sleep_forever();
		}
		_attached = true;
	}
	Kernel::irq_wait();
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc),
	_entrypoint(cap_session, STACK_SIZE, "irq"),
	_attached(false)
{
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (!_irq_alloc || (irq_number == -1) ||
	    _irq_alloc->alloc_addr(1, irq_number).is_error())
	{
		PERR("unavailable IRQ %lx requested", irq_number);
		return;
	}
	_irq_number = irq_number;
	_entrypoint.activate();
	_cap = Irq_session_capability(_entrypoint.manage(this));
}


Irq_session_component::~Irq_session_component()
{
	_irq_alloc->free((void*)_irq_number, 1);
	if (_attached) {
		if(Kernel::irq_free(_irq_number)){
			PERR("Kernel::irq_free failed");
		}
	}
}

