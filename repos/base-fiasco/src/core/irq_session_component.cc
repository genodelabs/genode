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
#include <base/log.h>
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

		if (L4_IPC_IS_ERROR(result)) error("Ipc error ", L4_IPC_ERROR(result));
	} while (L4_IPC_IS_ERROR(result));
}


void Irq_object::start()
{
	::Thread::start();
	_sync_bootup.lock();
}


void Irq_object::entry()
{
	if (!_associate()) {
		error("Could not associate with IRQ ", _irq);
		return;
	}

	/* thread is up and ready */
	_sync_bootup.unlock();

	/* wait for first ack_irq */
	_sync_ack.lock();

	while (true) {

		_wait_for_irq();

		if (!_sig_cap.valid())
			continue;

		Genode::Signal_transmitter(_sig_cap).submit(1);

		_sync_ack.lock();
	}
}


Irq_object::Irq_object(unsigned irq)
:
	Thread_deprecated<4096>("irq"),
	_sync_ack(Lock::LOCKED), _sync_bootup(Lock::LOCKED),
	_irq(irq)
{ }


Irq_session_component::Irq_session_component(Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_number(Arg_string::find_arg(args, "irq_number").long_value(-1)),
	_irq_alloc(irq_alloc),
	_irq_object(_irq_number)
{
	long msi = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (msi)
		throw Service_denied();

	if (!irq_alloc || irq_alloc->alloc_addr(1, _irq_number).error()) {
		error("unavailable IRQ ", _irq_number, " requested");
		throw Service_denied();
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component()
{
	error("Not yet implemented.");
}


void Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}


void Irq_session_component::sigh(Genode::Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Genode::Irq_session::Info Irq_session_component::info()
{
	/* no MSI support */
	return { .type = Info::Type::INVALID, .address = 0, .value = 0 };
}
