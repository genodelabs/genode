/*
 * \brief  Core implementation of IRQ sessions
 * \author Christian Helmuth
 * \date   2007-09-13
 *
 * FIXME ram quota missing
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
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

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/types.h>
}

using namespace Genode;


bool Irq_session_component::Irq_control_component::associate_to_irq(unsigned irq_number)
{
	using namespace Fiasco;

	int err;
	l4_threadid_t irq_tid;
	l4_umword_t dw0, dw1;
	l4_msgdope_t result;

	l4_make_taskid_from_irq(irq_number, &irq_tid);

	/* boost thread to IRQ priority */
	enum { IRQ_PRIORITY = 0xC0 };

	l4_sched_param_t param = {sp:{prio:IRQ_PRIORITY, small:0, state:0, time_exp:0, time_man:0}};
	l4_threadid_t    ext_preempter = L4_INVALID_ID;
	l4_threadid_t    partner       = L4_INVALID_ID;
	l4_sched_param_t old_param;
	l4_thread_schedule(l4_myself(), param, &ext_preempter, &partner, &old_param);

	err = l4_ipc_receive(irq_tid,
	                     L4_IPC_SHORT_MSG, &dw0, &dw1,
	                     L4_IPC_BOTH_TIMEOUT_0, &result);

	if (err != L4_IPC_RETIMEOUT) PERR("IRQ association failed");

	return (err == L4_IPC_RETIMEOUT);
}


void Irq_session_component::wait_for_irq()
{
	using namespace Fiasco;

	l4_threadid_t irq_tid;
	l4_umword_t dw0=0, dw1=0;
	l4_msgdope_t result;

	l4_make_taskid_from_irq(_irq_number, &irq_tid);

	do {
		l4_ipc_call(irq_tid,
		            L4_IPC_SHORT_MSG, 0, 0,
		            L4_IPC_SHORT_MSG, &dw0, &dw1,
		            L4_IPC_NEVER, &result);

		if (L4_IPC_IS_ERROR(result)) PERR("Ipc error %lx", L4_IPC_ERROR(result));
	} while (L4_IPC_IS_ERROR(result));
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc),
	_ep(cap_session, STACK_SIZE, "irqctrl"),
	_control_cap(_ep.manage(&_control_component)),
	_control_client(_control_cap)
{
	bool shared = Arg_string::find_arg(args, "irq_shared").bool_value(false);
	if (shared) {
		PWRN("IRQ sharing not supported");
		throw Root::Invalid_args();
	}

	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1 || !irq_alloc ||
	    irq_alloc->alloc_addr(1, irq_number).is_error()) {
		PERR("Unavailable IRQ %lx requested", irq_number);
		throw Root::Invalid_args();
	}
	_irq_number = irq_number;

	if (!_control_client.associate_to_irq(irq_number)) {
		PWRN("IRQ association failed");
		throw Root::Invalid_args();
	}

	/* initialize capability */
	_irq_cap = Irq_session_capability(_ep.manage(this));
}


Irq_session_component::~Irq_session_component()
{
	PERR("Implement me, immediately!");
}

