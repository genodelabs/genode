/*
 * \brief  Implementation of the SIGNAL service on the HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <unmanaged_singleton.h>

/* base-hw includes */
#include <kernel/core_interface.h>

/* core includes */
#include <signal_session_component.h>

using namespace Genode;


Signal_receiver_capability Signal_session_component::alloc_receiver()
{
	/* allocate resources for receiver */
	void * p;
	if (!_receivers_slab.alloc(Receiver::size(), &p)) {
		PERR("failed to allocate signal-receiver resources");
		throw Out_of_metadata();
	}
	/* create kernel object for receiver */
	addr_t donation = Receiver::kernel_donation(p);
	unsigned const id = Kernel::new_signal_receiver(donation);
	if (!id)
	{
		/* clean up */
		_receivers_slab.free(p, Receiver::size());
		PERR("failed to create signal receiver");
		throw Create_receiver_failed();
	}
	/* remember receiver ressources */
	Native_capability cap(id, id);
	Receiver * const r = new (p) Receiver(cap);
	_receivers.insert(r);

	/* return receiver capability */
	return reinterpret_cap_cast<Signal_receiver>(cap);
}


void Signal_session_component::free_receiver(Signal_receiver_capability cap)
{
	/* look up ressource info */
	Receiver * const r = _receivers.lookup_and_lock(cap);
	if (!r) {
		PERR("unknown signal receiver");
		throw Kill_receiver_failed();
	}
	/* release resources */
	_destruct_receiver(r);
	_receivers_slab.free(r, Receiver::size());
}


Signal_context_capability
Signal_session_component::alloc_context(Signal_receiver_capability r,
                                        unsigned const imprint)
{
	/* allocate resources for context */
	void * p;
	if (!_contexts_slab.alloc(Context::size(), &p)) {
		PERR("failed to allocate signal-context resources");
		throw Out_of_metadata();
	}
	/* create kernel object for context */
	addr_t donation = Context::kernel_donation(p);
	unsigned const id = Kernel::new_signal_context(donation, r.dst(), imprint);
	if (!id)
	{
		/* clean up */
		_contexts_slab.free(p, Context::size());
		PERR("failed to create signal context");
		throw Create_context_failed();
	}
	/* remember context ressources */
	Native_capability cap(id, id);
	_contexts.insert(new (p) Context(cap));

	/* return context capability */
	return reinterpret_cap_cast<Signal_context>(cap);
}


void Signal_session_component::free_context(Signal_context_capability cap)
{
	/* look up ressource info */
	Context * const c = _contexts.lookup_and_lock(cap);
	if (!c) {
		PERR("unknown signal context");
		throw Kill_context_failed();
	}
	/* release resources */
	_destruct_context(c);
	_contexts_slab.free(c, Context::size());
}


void Signal_session_component::_destruct_context(Context * const c)
{
	/* release kernel resources */
	if (Kernel::bin_signal_context(c->id()))
	{
		/* clean-up */
		c->release();
		PERR("failed to kill signal context");
		throw Kill_context_failed();
	}
	/* release core resources */
	_contexts.remove_locked(c);
	c->~Signal_session_context();
}


void Signal_session_component::_destruct_receiver(Receiver * const r)
{
	/* release kernel resources */
	if (Kernel::bin_signal_receiver(r->id()))
	{
		/* clean-up */
		r->release();
		PERR("failed to kill signal receiver");
		throw Kill_receiver_failed();
	}
	/* release core resources */
	_receivers.remove_locked(r);
	r->~Signal_session_receiver();
}
