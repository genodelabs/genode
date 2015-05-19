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


Signal_session_component::Receiver::Receiver()
: Kernel_object<Kernel::Signal_receiver>(true),
  Signal_session_component::Receiver::Pool::Entry(Kernel_object<Kernel::Signal_receiver>::_cap) { }


Signal_session_component::Context::Context(Signal_session_component::Receiver &r,
                                           unsigned const imprint)
: Kernel_object<Kernel::Signal_context>(true, r.kernel_object(), imprint),
  Signal_session_component::Context::Pool::Entry(Kernel_object<Kernel::Signal_context>::_cap) { }


Signal_receiver_capability Signal_session_component::alloc_receiver()
{
	try {
		Receiver * r = new (_receivers_slab) Receiver();
		_receivers.insert(r);
		return reinterpret_cap_cast<Signal_receiver>(r->cap());
	} catch (Allocator::Out_of_memory&) {
		PERR("failed to allocate signal-receiver resources");
		throw Out_of_metadata();
	}
	return reinterpret_cap_cast<Signal_receiver>(Untyped_capability());
}


void Signal_session_component::free_receiver(Signal_receiver_capability cap)
{
	/* look up ressource info */
	Receiver::Pool::Guard r(_receivers.lookup_and_lock(cap));
	if (!r) {
		PERR("unknown signal receiver");
		throw Kill_receiver_failed();
	}
	/* release resources */
	_receivers.remove_locked(r);
	destroy(&_receivers_slab, r.object());
}


Signal_context_capability
Signal_session_component::alloc_context(Signal_receiver_capability src,
                                        unsigned const imprint)
{
	/* look up ressource info */
	Receiver::Pool::Guard r(_receivers.lookup_and_lock(src));
	if (!r) {
		PERR("unknown signal receiver");
		throw Create_context_failed();
	}

	try {
		Context * c = new (_contexts_slab) Context(*r.object(), imprint);
		_contexts.insert(c);
		return reinterpret_cap_cast<Signal_context>(c->cap());
	} catch (Allocator::Out_of_memory&) {
		PERR("failed to allocate signal-context resources");
		throw Out_of_metadata();
	}
	return reinterpret_cap_cast<Signal_context>(Untyped_capability());
}


void Signal_session_component::free_context(Signal_context_capability cap)
{
	/* look up ressource info */
	Context::Pool::Guard c(_contexts.lookup_and_lock(cap));
	if (!c) {
		PERR("unknown signal context");
		throw Kill_context_failed();
	}
	/* release resources */
	_contexts.remove_locked(c);
	destroy(&_contexts_slab, c.object());
}


Signal_session_component::Signal_session_component(Allocator * const allocator,
                                                   size_t const quota)
: _allocator(allocator, quota), _receivers_slab(&_allocator),
  _contexts_slab(&_allocator) { }


Signal_session_component::~Signal_session_component()
{
	while (Context * const c = _contexts.first_locked()) {
		_contexts.remove_locked(c);
		destroy(&_contexts_slab, c);
	}
	while (Receiver * const r = _receivers.first_locked()) {
		_receivers.remove_locked(r);
		destroy(&_receivers_slab, r);
	}
}
