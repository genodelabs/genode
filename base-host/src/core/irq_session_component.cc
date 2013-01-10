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

/* core includes */
#include <irq_root.h>


using namespace Genode;


bool Irq_session_component::Irq_control_component::associate_to_irq(unsigned irq)
{
	PWRN("not implemented");
	return true;
}


void Irq_session_component::wait_for_irq()
{
	PWRN("not implemented");
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc),
	_ep(cap_session, STACK_SIZE, "irqctrl"),
	_irq_attached(false),
	_control_client(Capability<Irq_session_component::Irq_control>())
{
	PWRN("not implemented");
}


Irq_session_component::~Irq_session_component()
{
	PERR("not yet implemented");
}

