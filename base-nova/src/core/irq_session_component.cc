/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <irq_root.h>
#include <platform.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>
#include <nova_util.h>

using namespace Genode;


void Irq_session_component::wait_for_irq()
{
	if (Nova::sm_ctrl(_irq_number, Nova::SEMAPHORE_DOWN))
		nova_die();
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc),
	_ep(cap_session, STACK_SIZE, "irq")
{
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1 || !irq_alloc ||
	    irq_alloc->alloc_addr(1, irq_number) != Range_allocator::ALLOC_OK) {
		PERR("Unavailable IRQ %lx requested", irq_number);
		throw Root::Invalid_args();
	}

	/* alloc slector where IRQ will be mapped */
	_irq_number = cap_selector_allocator()->alloc();

	/* since we run in APIC mode translate IRQ 0 (PIT) to 2 */
	if (!irq_number)
		irq_number = 2;

	/* map IRQ number to selector */
	int ret = map_local((Nova::Utcb *)Thread_base::myself()->utcb(),
	                    Nova::Obj_crd(platform_specific()->gsi_base_sel() + irq_number, 0),
	                    Nova::Obj_crd(_irq_number, 0),
	                    true);
	if (ret) {
		PERR("Could not map IRQ %d", _irq_number);
		throw Root::Unavailable();
	}

	/* assign IRQ to CPU */
	enum { CPU = 0 };
	Nova::assign_gsi(_irq_number, 0, CPU);
	/* initialize capability */
	_irq_cap = _ep.manage(this);
}


Irq_session_component::~Irq_session_component() { }
