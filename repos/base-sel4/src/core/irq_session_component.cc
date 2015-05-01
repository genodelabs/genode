/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \date   2015-05-01
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
#include <irq_root.h>


using namespace Genode;


bool Irq_object::_associate() { return true; }


void Irq_object::_wait_for_irq()
{
	PDBG("not implemented");
}


void Irq_object::start()
{
	PDBG("not implemented");
}


void Irq_object::entry()
{
	PDBG("not implemented");
}


Irq_object::Irq_object(unsigned irq)
:
	Thread<4096>("irq"),
	_sync_ack(Lock::LOCKED), _sync_bootup(Lock::LOCKED),
	_irq(irq)
{ }


Irq_session_component::Irq_session_component(Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_number(Arg_string::find_arg(args, "irq_number").long_value(-1)),
	_irq_alloc(irq_alloc),
	_irq_object(_irq_number)
{
	long msi = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (msi)
		throw Root::Unavailable();

	if (!irq_alloc || irq_alloc->alloc_addr(1, _irq_number).is_error()) {
		PERR("Unavailable IRQ 0x%x requested", _irq_number);
		throw Root::Unavailable();
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component()
{
	PERR("Not yet implemented.");
}


void Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}


void Irq_session_component::sigh(Genode::Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Genode::Irq_session::Info Irq_session_component::info()
{
	/* no MSI support */
	return { .type = Genode::Irq_session::Info::Type::INVALID };
}
