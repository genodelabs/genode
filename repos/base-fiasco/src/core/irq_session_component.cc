/*
 * \brief  Core implementation of IRQ sessions
 * \author Christian Helmuth
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <irq_args.h>
#include <util.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

using namespace Core;


bool Irq_object::_associate()
{
	using namespace Fiasco;

	int err;
	l4_threadid_t irq_tid;
	l4_umword_t dw0, dw1;
	l4_msgdope_t result;

	l4_make_taskid_from_irq(_irq, &irq_tid);

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

	if (err != L4_IPC_RETIMEOUT) error("IRQ association failed");

	return (err == L4_IPC_RETIMEOUT);
}


void Irq_object::_wait_for_irq()
{
	using namespace Fiasco;

	l4_threadid_t irq_tid;
	l4_umword_t dw0=0, dw1=0;
	l4_msgdope_t result;

	l4_make_taskid_from_irq(_irq, &irq_tid);

	do {
		l4_ipc_call(irq_tid,
		            L4_IPC_SHORT_MSG, 0, 0,
		            L4_IPC_SHORT_MSG, &dw0, &dw1,
		            L4_IPC_NEVER, &result);

		if (L4_IPC_IS_ERROR(result))
			error("Ipc error ", L4_IPC_ERROR(result));

	} while (L4_IPC_IS_ERROR(result));
}


Thread::Start_result Irq_object::start()
{
	Start_result const result = ::Thread::start();
	_sync_bootup.block();
	return result;
}


void Irq_object::entry()
{
	if (!_associate()) {
		error("Could not associate with IRQ ", _irq);
		return;
	}

	/* thread is up and ready */
	_sync_bootup.wakeup();

	/* wait for first ack_irq */
	_sync_ack.block();

	while (true) {

		_wait_for_irq();

		if (!_sig_cap.valid())
			continue;

		Signal_transmitter(_sig_cap).submit(1);

		_sync_ack.block();
	}
}


Irq_object::Irq_object(unsigned irq)
:
	Thread(Weight::DEFAULT_WEIGHT, "irq", 4096 /* stack */, Type::NORMAL),
	_irq(irq)
{ }


static Range_allocator::Result allocate(Range_allocator &irq_alloc, Irq_args const &args)
{
	if (args.msi())
		return Alloc_error::DENIED;

	return irq_alloc.alloc_addr(1, args.irq_number());
}


Irq_session_component::Irq_session_component(Range_allocator &irq_alloc,
                                             const char      *args)
:
	_irq_number(allocate(irq_alloc, Irq_args(args))),
	_irq_object(Irq_args(args).irq_number())
{
	if (_irq_number.failed()) {
		error("unavailable IRQ ", Irq_args(args).irq_number(), " requested");
		return;
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component() { }


void Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}


void Irq_session_component::sigh(Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Irq_session::Info Irq_session_component::info()
{
	/* no MSI support */
	return { .type = Info::Type::INVALID, .address = 0, .value = 0 };
}
