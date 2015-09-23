/*
 * \brief  Implementation of the SIGNAL interface
 * \author Alexander Boettcher
 * \date   2015-03-04
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <signal_session_component.h>

using namespace Genode;


/******************************
 ** Signal-session component **
 ******************************/

Signal_session_component::Signal_session_component(Rpc_entrypoint *source_ep,
                                                   Rpc_entrypoint *context_ep,
                                                   Allocator      *context_md_alloc,
                                                   size_t          ram_quota)
:
	_source_ep(source_ep),
	_source(source_ep),
	_source_cap(_source_ep->manage(&_source)),
	_md_alloc(context_md_alloc, ram_quota),
	_contexts_slab(&_md_alloc)
{ }


Signal_session_component::~Signal_session_component()
{
	/* remove _signal_source from entrypoint */
	_source_ep->dissolve(&_source);

	/* free all signal contexts */
	while (Signal_context_component *r = _contexts_slab.first_object())
		free_context(reinterpret_cap_cast<Signal_context>(Native_capability(r->cap())));
}


Signal_source_capability Signal_session_component::signal_source()
{
	return _source_cap;
}

extern "C" Genode::addr_t __core_pd_sel;

Signal_context_capability Signal_session_component::alloc_context(long imprint)
{
	Native_capability sm = _source._blocking_semaphore;

	if (!sm.valid()) {
		PWRN("signal receiver sm is not valid");
		return Signal_context_capability();
	}

	Native_capability si(cap_map()->insert());
	Signal_context_capability cap = reinterpret_cap_cast<Signal_context>(si);

	uint8_t res = Nova::create_si(cap.local_name(), __core_pd_sel, imprint,
	                              sm.local_name());
	if (res != Nova::NOVA_OK) {
		PWRN("creating signal failed - error %u", res);
		return Signal_context_capability();
	}

	try {
		_signal_queue.insert(new (&_contexts_slab) Signal_context_component(cap));
	} catch (Allocator::Out_of_memory) {
		throw Out_of_metadata();
	}

	/* return unique capability for the signal context */
	return cap;
}


void Signal_session_component::free_context(Signal_context_capability context_cap)
{
	Signal_context_component *context;
	auto lambda = [&] (Signal_context_component *c) {
		context = c;
		if (context) _signal_queue.remove(context);
	};
	_signal_queue.apply(context_cap, lambda);

	if (!context) {
		PWRN("%p - specified signal-context capability has wrong type %lx",
			 this, context_cap.local_name());
		return;
	}
	destroy(&_contexts_slab, context);

	Nova::revoke(Nova::Obj_crd(context_cap.local_name(), 0));
	cap_map()->remove(context_cap.local_name(), 0);
}


void Signal_session_component::submit(Signal_context_capability context_cap,
                                      unsigned                  cnt)
{
	PDBG("should not be called");
}
