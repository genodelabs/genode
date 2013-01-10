/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <irq_root.h>

/* Codezero includes */
#include <codezero/syscalls.h>


using namespace Genode;


void Irq_session_component::wait_for_irq()
{
	using namespace Codezero;

	/* attach thread to IRQ when first called */
	if (!_attached) {
		int ret = l4_irq_control(IRQ_CONTROL_REGISTER, 0, _irq_number);
		if (ret < 0) {
			PERR("l4_irq_control(IRQ_CONTROL_REGISTER) returned %d", ret);
			sleep_forever();
		}
		_attached = true;
	}

	/* block for IRQ */
	int ret = l4_irq_control(IRQ_CONTROL_WAIT, 0, _irq_number);
	if (ret < 0)
		PWRN("l4_irq_control(IRQ_CONTROL_WAIT) returned %d", ret);
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
	if (!irq_alloc || (irq_number == -1)||
	    irq_alloc->alloc_addr(1, irq_number).is_error()) {
		PERR("unavailable IRQ %lx requested", irq_number);
		return;
	}
	_irq_number = irq_number;
	_cap = Irq_session_capability(_entrypoint.manage(this));
}


Irq_session_component::~Irq_session_component()
{
	PERR("not yet implemented");
}

