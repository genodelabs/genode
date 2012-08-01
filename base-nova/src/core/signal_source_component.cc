/*
 * \brief  Implementation of the SIGNAL interface
 * \author Norman Feske
 * \date   2009-08-11
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/printf.h>
#include <base/cap_sel_alloc.h>

/* core includes */
#include <signal_session_component.h>
#include <platform_pd.h>

/* NOVA includes */
#include <nova/syscalls.h>

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
		Nova::sm_ctrl(_blocking_semaphore.local_name(),
		              Nova::SEMAPHORE_UP);
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
	/* initialized blocking semaphore */
	addr_t sem_sel = cap_selector_allocator()->alloc();
	uint8_t ret = Nova::create_sm(sem_sel,
	                              Platform_pd::pd_core_sel(), 0);
	if (ret)
		PERR("create_sm returned %u", ret);

	_blocking_semaphore = Native_capability(sem_sel);
}
