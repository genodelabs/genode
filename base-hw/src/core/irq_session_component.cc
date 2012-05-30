/*
 * \brief  Implementation of IRQ session component
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <kernel/syscalls.h>

/* core includes */
#include <irq_root.h>

using namespace Genode;


bool
Irq_session_component::Irq_control_component::associate_to_irq(unsigned irq)
{ return Kernel::allocate_irq(irq); }


void Irq_session_component::wait_for_irq() { Kernel::await_irq(); }


Irq_session_component::~Irq_session_component()
{
	/* free IRQ for other threads */
	if (Kernel::free_irq(_irq_number))
		PERR("Could not free IRQ %u", _irq_number);
}


Irq_session_component::Irq_session_component(Cap_session *     cap_session,
                                             Range_allocator * irq_alloc,
                                             const char *      args)
:
	_irq_alloc(irq_alloc), _ep(cap_session, STACK_SIZE, "irqctrl"),
	_control_cap(_ep.manage(&_control_component)),
	_control_client(_control_cap)
{
	/* check arguments */
	bool shared = Arg_string::find_arg(args, "irq_shared").bool_value(false);
	if (shared) {
		PERR("IRQ sharing not supported");
		throw Root::Invalid_args();
	}
	/* allocate IRQ */
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1 || !irq_alloc ||
	    irq_alloc->alloc_addr(1, irq_number) != Range_allocator::ALLOC_OK) {
		PERR("Unavailable IRQ %lu requested", irq_number);
		throw Root::Invalid_args();
	}
	_irq_number = irq_number;

	/* configure control client */
	if (!_control_client.associate_to_irq(irq_number)) {
		PERR("IRQ association failed");
		throw Root::Invalid_args();
	}
	/* create IRQ capability */
	_irq_cap = Irq_session_capability(_ep.manage(this));
}
