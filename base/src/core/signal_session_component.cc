/*
 * \brief  Implementation of the SIGNAL interface
 * \author Norman Feske
 * \date   2009-08-11
 */

/*
 * Copyright (C) 2009-2011 Genode Labs GmbH
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
	/* free all signal contexts */
	while (Signal_context_component *r = _contexts_slab.first_object())
		free_context(r->cap());

	/* remove _signal_source from entrypoint */
	_source_ep->dissolve(&_source);
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
	Signal_context_component *context;
	context = dynamic_cast<Signal_context_component *>
	          (_context_ep->obj_by_cap(context_cap));

	if (!context) {
		PWRN("specified signal-context capability has wrong type");
		return;
	}

	_context_ep->dissolve(context);
	destroy(&_contexts_slab, context);
}


void Signal_session_component::submit(Signal_context_capability context_cap,
                                      unsigned                   cnt)
{
	Signal_context_component *context;
	context = dynamic_cast<Signal_context_component *>
	           (_context_ep->obj_by_cap(context_cap));

	if (!context) {
		PWRN("invalid signal-context capability");
		return;
	}

	context->source()->submit(context, _ipc_ostream, cnt);
}
