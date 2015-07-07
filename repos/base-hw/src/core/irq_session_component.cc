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


unsigned Irq_session_component::_find_irq_number(const char * const args)
{
	return Arg_string::find_arg(args, "irq_number").long_value(-1);
}


void Irq_session_component::ack_irq()
{
	using Kernel::User_irq;
	if (!_sig_cap.valid()) { return; }
	User_irq * const kirq = reinterpret_cast<User_irq*>(&_kernel_object);
	Kernel::ack_irq(kirq);
}


void Irq_session_component::sigh(Signal_context_capability cap)
{
	if (_sig_cap.valid()) {
		PWRN("signal handler already registered for IRQ %u", _irq_number);
		return;
	}

	_sig_cap = cap;

	if (Kernel::new_irq((addr_t)&_kernel_object, _irq_number, _sig_cap.dst()))
		PWRN("invalid signal handler for IRQ %u", _irq_number);
}


Irq_session_component::~Irq_session_component()
{
	using namespace Kernel;

	User_irq * kirq = reinterpret_cast<User_irq*>(&_kernel_object);
	_irq_alloc->free((void *)_irq_number);
	if (_sig_cap.valid())
		Kernel::delete_irq(kirq);
}


Irq_session_component::Irq_session_component(Range_allocator * const irq_alloc,
                                             const char      * const      args)
:
	_irq_number(Platform::irq(_find_irq_number(args))), _irq_alloc(irq_alloc)
{
	long const msi = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (msi)
		throw Root::Unavailable();

	/* allocate interrupt */
	if (_irq_alloc->alloc_addr(1, _irq_number).is_error()) {
		PERR("unavailable interrupt %d requested", _irq_number);
		throw Root::Invalid_args();
	}

	long irq_trg = Arg_string::find_arg(args, "irq_trigger").long_value(-1);
	long irq_pol = Arg_string::find_arg(args, "irq_polarity").long_value(-1);

	Irq_session::Trigger irq_trigger;
	Irq_session::Polarity irq_polarity;

	switch(irq_trg) {
	case -1:
	case Irq_session::TRIGGER_UNCHANGED:
		irq_trigger = Irq_session::TRIGGER_UNCHANGED;
		break;
	case Irq_session::TRIGGER_EDGE:
		irq_trigger = Irq_session::TRIGGER_EDGE;
		break;
	case Irq_session::TRIGGER_LEVEL:
		irq_trigger = Irq_session::TRIGGER_LEVEL;
		break;
	default:
		PERR("invalid trigger mode %ld specified for IRQ %u", irq_trg,
		     _irq_number);
		throw Root::Unavailable();
	}

	switch(irq_pol) {
	case -1:
	case POLARITY_UNCHANGED:
		irq_polarity = POLARITY_UNCHANGED;
		break;
	case POLARITY_HIGH:
		irq_polarity = POLARITY_HIGH;
		break;
	case POLARITY_LOW:
		irq_polarity = POLARITY_LOW;
		break;
	default:
		PERR("invalid polarity %ld specified for IRQ %u", irq_pol,
		     _irq_number);
		throw Root::Unavailable();
	}

	Platform::setup_irq_mode(_irq_number, irq_trigger, irq_polarity);
}
