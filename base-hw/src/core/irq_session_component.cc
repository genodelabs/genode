/*
 * \brief  Backend for IRQ sessions served by core
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base-hw includes */
#include <kernel/interface.h>

/* core includes */
#include <kernel/irq.h>
#include <irq_root.h>
#include <core_env.h>

using namespace Genode;

/**
 * On other platforms, every IRQ session component creates its entrypoint.
 * However, on base-hw this isn't necessary as users can wait for their
 * interrupts directly. Instead of replacing cores generic irq_root.h and
 * main.cc with base-hw specific versions, we simply use a local singleton.h
 */
static Rpc_entrypoint * irq_session_ep()
{
	enum { STACK_SIZE = 2048 };
	static Rpc_entrypoint
		_ep(core_env()->cap_session(), STACK_SIZE, "irq_session_ep");
	return &_ep;
}

void Irq_session_component::wait_for_irq() { PERR("not implemented"); }

Irq_signal Irq_session_component::signal() { return _signal; }

Irq_session_component::~Irq_session_component() { PERR("not implemented"); }


Irq_session_component::Irq_session_component(Cap_session * const     cap_session,
                                             Range_allocator * const irq_alloc,
                                             const char * const      args)
:
	_irq_alloc(irq_alloc)
{
	/* check arguments */
	bool shared = Arg_string::find_arg(args, "irq_shared").bool_value(false);
	if (shared) {
		PERR("shared interrupts not supported");
		throw Root::Invalid_args();
	}
	/* allocate interrupt */
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	bool error = irq_number < 0 || !_irq_alloc;
	error |= _irq_alloc->alloc_addr(1, irq_number).is_error();
	if (error) {
		PERR("unavailable interrupt requested");
		throw Root::Invalid_args();
	}
	/* make interrupt accessible */
	_signal = Irq::signal(irq_number);
	_cap    = Irq_session_capability(irq_session_ep()->manage(this));
}
