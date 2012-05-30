/*
 * \brief  Implementation of the SIGNAL service on the HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <kernel/syscalls.h>

/* core includes */
#include <signal_session_component.h>

using namespace Genode;

enum {
	RECEIVER_SLAB_CHUNK_SIZE = 32,
	CONTEXT_SLAB_CHUNK_SIZE = 32,
};


Signal_session_component::Signal_session_component(Allocator * const md,
                                                   size_t const ram_quota) :
	_md_alloc(md, ram_quota),
	_receivers_slab(Kernel::signal_receiver_size(),
	                RECEIVER_SLAB_CHUNK_SIZE * Kernel::signal_receiver_size(),
	                0, &_md_alloc),

	_contexts_slab(Kernel::signal_context_size(),
	               CONTEXT_SLAB_CHUNK_SIZE * Kernel::signal_context_size(),
	               0, &_md_alloc)
{ }


Signal_session_component::~Signal_session_component()
{
	PERR("%s: Not implemented", __PRETTY_FUNCTION__);
	while (1) ;
}


Signal_receiver_capability Signal_session_component::alloc_receiver()
{
	/* create receiver kernel-object */
	size_t const s = Kernel::signal_receiver_size();
	void * p;
	if (!_receivers_slab.alloc(s, &p)) throw Out_of_metadata();
	unsigned long const id = Kernel::new_signal_receiver(p);
	if (!id) throw Out_of_metadata();

	/* return reference to the new kernel-object */
	Native_capability c(id, 0);
	return reinterpret_cap_cast<Signal_receiver>(c);
}


Signal_context_capability
Signal_session_component::alloc_context(Signal_receiver_capability r,
                                        unsigned long imprint)
{
	/* create context kernel-object */
	size_t const s = Kernel::signal_context_size();
	void * p;
	if (!_contexts_slab.alloc(s, &p)) throw Out_of_metadata();
	unsigned long const id = Kernel::new_signal_context(p, r.dst(), imprint);
	if (!id) throw Out_of_metadata();

	/* return reference to the new kernel-object */
	Native_capability c(id, 0);
	return reinterpret_cap_cast<Signal_context>(c);
}

