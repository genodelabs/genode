/*
 * \brief  Pistachio-specific implementation of IRQ sessions
 * \author Julian Stecklina
 * \date   2008-02-21
 *
 * FIXME ram quota missing
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <util.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/ipc.h>
}

using namespace Genode;
using namespace Pistachio;


static inline L4_ThreadId_t irqno_to_threadid(unsigned int irqno)
{
	/*
	 * Interrupt threads have their number as thread_no and a version of 1.
	 */
	return L4_GlobalId(irqno, 1);
}


bool Irq_session_component::Irq_control_component::associate_to_irq(unsigned)
{
	/*
	 * We defer the association with the IRQ to the first call of the
	 * 'wait_for_irq' function.
	 */
	return true;
}


void Irq_session_component::wait_for_irq()
{
	L4_ThreadId_t irq_thread = irqno_to_threadid(_irq_number);

	/* attach to IRQ when called for the first time */
	L4_MsgTag_t res;
	if (!_irq_attached) {

		if (L4_AssociateInterrupt(irq_thread, L4_Myself()) != true) {
			PERR("L4_AssociateInterrupt failed");
			return;
		}

		/*
		 * Right after associating with an interrupt, the interrupt is
		 * unmasked. Hence, we do not need to send an unmask message
		 * to the IRQ thread but just wait for the IRQ.
		 */
		L4_Set_MsgTag(L4_Niltag);
		res = L4_Receive(irq_thread);

		/*
		 * Now, the IRQ is masked. To receive the next IRQ we have to send
		 * an unmask message to the IRQ thread first.
		 */
		_irq_attached = true;

	/* receive subsequent interrupt */
	} else {

		/* send unmask message and wait for new IRQ */
		L4_Set_MsgTag(L4_Niltag);
		res = L4_Call(irq_thread);
	}

	if (L4_IpcFailed(res)) {
		PERR("ipc error while waiting for interrupt.");
		return;
	}
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
	bool shared = Arg_string::find_arg(args, "irq_shared").bool_value(false);
	if (shared) {
		PWRN("IRQ sharing not supported");

		/* FIXME error condition -> exception */
		return;
	}

	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1 || !irq_alloc ||
	    irq_alloc->alloc_addr(1, irq_number).is_error()) {
		PERR("unavailable IRQ %lx requested", irq_number);

		/* FIXME error condition -> exception */
		return;
	}
	_irq_number = irq_number;

	/* initialize capability */
	_irq_cap = _ep.manage(this);
}


Irq_session_component::~Irq_session_component()
{
	L4_Word_t res = L4_DeassociateInterrupt(irqno_to_threadid(_irq_number));

	if (res != 1) {
		PERR("L4_DeassociateInterrupt failed");
	}
}

