/*
 * \brief  Implementation of the SIGNAL interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-08-11
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/cap_sel_alloc.h>
#include <base/native_types.h>

/* core includes */
#include <signal_session_component.h>
#include <cap_session_component.h>

namespace Fiasco {
#include <l4/sys/factory.h>
#include <l4/sys/irq.h>
}

using namespace Genode;


/*****************************
 ** Signal-source component **
 *****************************/

void Signal_source_component::submit(Signal_context_component *context,
                                     Ipc_ostream              *ostream,
                                     int                       cnt)
{
	/* enqueue signal to context */
	context->increment_signal_cnt(cnt);

	if (!context->is_enqueued()) {
		_signal_queue.enqueue(context);

		/* wake up client */
		Fiasco::l4_irq_trigger(_blocking_semaphore.tid());
	}
}


Signal_source::Signal Signal_source_component::wait_for_signal()
{
	if (_signal_queue.empty()) {
		PWRN("unexpected call of wait_for_signal");
		return Signal(0, 0);
	}

	/* dequeue and return pending signal */
	Signal_context_component *context = _signal_queue.dequeue();
	Signal result(context->imprint(), context->cnt());
	context->reset_signal_cnt();
	return result;
}


Signal_source_component::Signal_source_component(Rpc_entrypoint *ep)
: _entrypoint(ep)
{
	using namespace Fiasco;

	unsigned long    badge = Badge_allocator::allocator()->alloc();
	Native_thread_id irq   = cap_alloc()->alloc_id(badge);
	l4_msgtag_t      res   = l4_factory_create_irq(L4_BASE_FACTORY_CAP, irq);
	if (l4_error(res))
		PERR("Allocation of irq object failed!");

	_blocking_semaphore = Native_capability(irq, badge);
}
