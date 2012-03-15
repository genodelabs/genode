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
#include <base/native_types.h>

/* core includes */
#include <signal_session_component.h>
#include <platform.h>

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
		Fiasco::l4_irq_trigger(_blocking_semaphore.dst());
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
: Signal_source_rpc_object(cap_map()->insert(platform_specific()->cap_id_alloc()->alloc())),
  _entrypoint(ep)
{
	using namespace Fiasco;

	l4_msgtag_t res = l4_factory_create_irq(L4_BASE_FACTORY_CAP,
	                                        _blocking_semaphore.dst());
	if (l4_error(res))
		PERR("Allocation of irq object failed!");
}
