/*
 * \brief  Backend for IRQ sessions served by core
 * \author Martin Stein
 * \author Reto Buerki
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base-hw includes */
#include <kernel/core_interface.h>

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

Irq_session_component::~Irq_session_component()
{
	irq_session_ep()->dissolve(this);
	_irq_alloc->free((void *)(addr_t)_irq_number);
}

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
	long irq_nr = Arg_string::find_arg(args, "irq_number").long_value(-1);
	bool error = irq_nr < 0 || !_irq_alloc;

	/* enable platform specific code to apply mappings */
	long const plat_irq_nr = Platform::irq(irq_nr);

	error |= _irq_alloc->alloc_addr(1, plat_irq_nr).is_error();
	if (error) {
		PERR("unavailable interrupt requested");
		throw Root::Invalid_args();
	}
	/* make interrupt accessible */
	_irq_number = (unsigned)plat_irq_nr;
	_signal = Kernel::User_irq::signal(plat_irq_nr);
	_cap    = Irq_session_capability(irq_session_ep()->manage(this));
}
