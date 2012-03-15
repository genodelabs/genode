/*
 * \brief  Fiasco.OC-specific core implementation of IRQ sessions
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2007-09-13
 *
 * FIXME ram quota missing
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <irq_session_component.h>
#include <platform.h>
#include <util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/icu.h>
#include <l4/sys/irq.h>
#include <l4/sys/factory.h>
#include <l4/sys/types.h>
}

using namespace Genode;


Irq_session_component::Interrupt*
Irq_session_component::Interrupt::find_by_num(unsigned num)
{
	if (number == num) return this;

	Interrupt *n = Avl_node<Interrupt>::child(num > number);
	return n ? n->find_by_num(num) : 0;
}


bool Irq_session_component::Interrupt::higher(Irq_session_component::Interrupt *n)
{
	return n->number > number;
}


Irq_session_component::Interrupt::Interrupt()
: _cap(cap_map()->insert(platform_specific()->cap_id_alloc()->alloc())),
  _sem(), number(0) {}


Native_thread Irq_session_component::Interrupt_handler::handler_cap()
{
	static Interrupt_handler handler;
	return handler._thread_cap.dst();
}


void Irq_session_component::Interrupt_handler::entry()
{
	using namespace Fiasco;

	int         err;
	l4_msgtag_t tag;
	l4_umword_t label;

	while (true) {
		tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
		if ((err = l4_ipc_error(tag, l4_utcb())))
			PERR("IRQ receive: %d\n", err);
		else {
			Interrupt *intr = _irqs()->first();
			if (intr) {
				intr = intr->find_by_num(label);
				if (intr)
					intr->semaphore()->up();
			}
		}
	}
}


Avl_tree<Irq_session_component::Interrupt>* Irq_session_component::_irqs()
{
	static Avl_tree<Interrupt> irqs;
	return &irqs;
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc),
	_ep(cap_session, STACK_SIZE, "irqctrl")
{
	using namespace Fiasco;

	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if ((irq_number == -1) || _irq_alloc->alloc_addr(1, irq_number)) {
		PERR("Unavailable IRQ %lx requested", irq_number);
		throw Root::Invalid_args();
	}

	/*
	 * temorary hack for fiasco.oc using the local-apic,
	 * where old pic-line 0 maps to 2
	 */
	if (irq_number == 0)
		irq_number = 2;

	_irq.number = irq_number;
	Irq_session_component::_irqs()->insert(&_irq);

	if (l4_error(l4_factory_create_irq(L4_BASE_FACTORY_CAP, _irq.capability())))
		PERR("l4_factory_create_irq failed!");

	if (l4_error(l4_icu_bind(L4_BASE_ICU_CAP, irq_number, _irq.capability())))
		PERR("Binding IRQ%ld to the ICU failed", irq_number);

	/* set interrupt mode */
	Platform::setup_irq_mode(irq_number);

	if (l4_error(l4_irq_attach(_irq.capability(), irq_number,
	                           Interrupt_handler::handler_cap())))
		PERR("Error attaching to IRQ %ld", irq_number);

	/* initialize capability */
	_irq_cap = _ep.manage(this);
}


void Irq_session_component::wait_for_irq()
{
	using namespace Fiasco;

	int err;
	l4_msgtag_t tag = l4_irq_unmask(_irq.capability());
	if ((err = l4_ipc_error(tag, l4_utcb())))
		PERR("IRQ unmask: %d\n", err);

	_irq.semaphore()->down();
}


Irq_session_component::~Irq_session_component()
{
	PERR("Implement me, immediately!");
}
