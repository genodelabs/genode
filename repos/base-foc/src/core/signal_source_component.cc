/*
 * \brief  Implementation of the SIGNAL interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-08-11
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/native_capability.h>

/* core includes */
#include <platform.h>
#include <signal_source_component.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Genode;


/*****************************
 ** Signal-source component **
 *****************************/

void Signal_source_component::release(Signal_context_component &context)
{
	if (context.enqueued())
		_signal_queue.remove(context);
}


void Signal_source_component::submit(Signal_context_component &context,
                                     unsigned long             cnt)
{
	/* enqueue signal to context */
	context.increment_signal_cnt((int)cnt);

	if (!context.enqueued()) {
		_signal_queue.enqueue(context);

		/* wake up client */
		Foc::l4_irq_trigger(_blocking_semaphore.data()->kcap());
	}
}


Signal_source::Signal Signal_source_component::wait_for_signal()
{
	if (_signal_queue.empty()) {
		warning("unexpected call of wait_for_signal");
		return Signal(0, 0);
	}

	/* dequeue and return pending signal */
	Signal result { };
	_signal_queue.dequeue([&result] (Signal_context_component &context) {
		result = Signal(context.imprint(), context.cnt());
		context.reset_signal_cnt();
	});
	return result;
}


Signal_source_component::Signal_source_component(Rpc_entrypoint &ep)
:
	Signal_source_rpc_object(cap_map().insert(platform_specific().cap_id_alloc().alloc())),
	_entrypoint(ep)
{
	using namespace Foc;

	l4_msgtag_t res = l4_factory_create_irq(L4_BASE_FACTORY_CAP,
	                                        _blocking_semaphore.data()->kcap());
	if (l4_error(res))
		error("Allocation of irq object failed!");
}


Signal_source_component::~Signal_source_component()
{
	/*
	 * On Fiasco.OC, the signal-source client does not use a blocking call
	 * to wait for signals. Hence, we do not need to take care of
	 * releasing the reply capability of such a call.
	 */
}
