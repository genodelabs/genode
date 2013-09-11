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
#include <base/sleep.h>
#include <kernel/syscalls.h>

/* core includes */
#include <signal_session_component.h>

using namespace Genode;


/**
 * Placement new
 */
void * operator new (size_t, void * p) { return p; }


Signal_session_component::Signal_session_component(Allocator * const md,
                                                   size_t const ram_quota) :
	_md_alloc(md, ram_quota),
	_receivers_slab(Kernel::signal_receiver_size(), RECEIVERS_SB_SIZE,
	                (Slab_block *)&_initial_receivers_sb, &_md_alloc),
	_contexts_slab(Context::slab_size(), CONTEXTS_SB_SIZE,
	               (Slab_block *)&_initial_contexts_sb, &_md_alloc)
{ }


Signal_session_component::~Signal_session_component()
{
	PERR("%s: Not implemented", __PRETTY_FUNCTION__);
	while (1) ;
}


Signal_receiver_capability Signal_session_component::alloc_receiver()
{
	/* allocate resources for receiver */
	size_t const s = Kernel::signal_receiver_size();
	void * p;
	if (!_receivers_slab.alloc(s, &p)) {
		PERR("failed to allocate signal receiver");
		throw Out_of_metadata();
	}
	/* create kernel object for receiver */
	unsigned const id = Kernel::new_signal_receiver(p);
	if (!id) {
		PERR("failed to create signal receiver");
		throw Exception();
	}
	/* return receiver capability */
	Native_capability c(id, id);
	return reinterpret_cap_cast<Signal_receiver>(c);
}


Signal_context_capability
Signal_session_component::alloc_context(Signal_receiver_capability r,
                                        unsigned const imprint)
{
	/* allocate resources for context */
	void * p;
	if (!_contexts_slab.alloc(Context::slab_size(), &p)) {
		PERR("failed to allocate signal-context resources");
		throw Out_of_metadata();
	}
	/* create kernel object for context */
	void * donation = Context::kernel_donation(p);
	unsigned const id = Kernel::new_signal_context(donation, r.dst(), imprint);
	if (!id)
	{
		/* clean up */
		_contexts_slab.free(p, Context::slab_size());
		PERR("failed to create signal context");
		throw Exception();
	}
	/* remember context ressources */
	Native_capability cap(id, id);
	_contexts.insert(new (p) Context(cap));

	/* return context capability */
	return reinterpret_cap_cast<Signal_context>(cap);
}


void Signal_session_component::free_context(Signal_context_capability cap)
{
	/* lookup ressource info */
	Context * const c = _contexts.lookup_and_lock(cap);
	if (!c) {
		PERR("unknown signal context");
		throw Exception();
	}
	/* release kernel resources */
	if (!Kernel::kill_signal_context(c->id()))
	{
		/* clean-up */
		c->release();
		PERR("failed to kill signal context");
		throw Exception();
	}
	/* release core resources */
	_contexts.remove_locked(c);
	c->~Context();
	_contexts_slab.free(c, Context::slab_size());
}
