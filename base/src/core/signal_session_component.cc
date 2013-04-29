/*
 * \brief  Implementation of the SIGNAL interface
 * \author Norman Feske
 * \date   2009-08-11
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
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
	_context_ep(context_ep),
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
		free_context(r->cap());
}


Signal_source_capability Signal_session_component::signal_source()
{
	return _source_cap;
}


Signal_context_capability Signal_session_component::alloc_context(long imprint)
{
	Signal_context_component *context;

	try { context = new (&_contexts_slab)
	                Signal_context_component(imprint, &_source); }

	catch (Allocator::Out_of_memory) { throw Out_of_metadata(); }

	/* return unique capability for the signal context */
	return _context_ep->manage(context);
}


void Signal_session_component::free_context(Signal_context_capability context_cap)
{
	Signal_context_component * context =
		dynamic_cast<Signal_context_component *>(_context_ep->lookup_and_lock(context_cap));
	if (!context) {
		PWRN("specified signal-context capability has wrong type");
		return;
	}

	_context_ep->dissolve(context);
	destroy(&_contexts_slab, context);
}


void Signal_session_component::submit(Signal_context_capability context_cap,
                                      unsigned                  cnt)
{
	Object_pool<Signal_context_component>::Guard
		context(_context_ep->lookup_and_lock(context_cap));
	if (!context) {
		/*
		 * We do not use PWRN() to enable the build system to suppress this
		 * warning in release mode (SPECS += release).
		 */
		PDBG("invalid signal-context capability");
		return;
	}

	context->source()->submit(context, _ipc_ostream, cnt);
}


Signal_context_component::~Signal_context_component()
{
	if (is_enqueued() && _source)
		_source->release(this);
}
