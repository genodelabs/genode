/*
 * \brief  Implementation of the SIGNAL interface
 * \author Alexander Boettcher
 * \date   2016-07-09
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <platform.h>
#include <signal_source_component.h>

/* base-internal include */
#include <core_capability_space.h>


using namespace Genode;


/*****************************
 ** Signal-source component **
 *****************************/

void Signal_source_component::release(Signal_context_component *context)
{
	if (context && context->enqueued())
		_signal_queue.remove(context);
}


void Signal_source_component::submit(Signal_context_component *context,
                                     unsigned long             cnt)
{
	/*
	 * If the client does not block in 'wait_for_signal', the
	 * signal will be delivered as result of the next
	 * 'wait_for_signal' call.
	 */
	context->increment_signal_cnt(cnt);

	if (context->enqueued())
		return;

	_signal_queue.enqueue(context);

	seL4_Signal(Capability_space::ipc_cap_data(_notify).sel.value());
}


Signal_source::Signal Signal_source_component::wait_for_signal()
{
	if (_signal_queue.empty())
		return Signal(0, 0);  /* just a dummy */

	/* dequeue and return pending signal */
	Signal_context_component *context = _signal_queue.dequeue();
	Signal result(context->imprint(), context->cnt());
	context->reset_signal_cnt();
	return result;
}


Signal_source_component::Signal_source_component(Rpc_entrypoint *ep)
:
	_entrypoint(ep)
{
	Platform        &platform   = *platform_specific();
	Range_allocator &phys_alloc = *platform.ram_alloc();

	addr_t       const phys_addr = Untyped_memory::alloc_page(phys_alloc);
	seL4_Untyped const service   = Untyped_memory::untyped_sel(phys_addr).value();

	/* allocate notification object within core's CNode */
	Cap_sel ny_sel = platform.core_sel_alloc().alloc();
	create<Notification_kobj>(service, platform.core_cnode().sel(), ny_sel);

	_notify = Capability_space::create_notification_cap(ny_sel);
}


Signal_source_component::~Signal_source_component() { }
