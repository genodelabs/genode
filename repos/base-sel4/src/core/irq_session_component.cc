/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <platform.h>
#include <irq_root.h>
#include <irq_args.h>

#include <sel4/sel4.h>

using namespace Genode;


bool Irq_object::associate(Irq_session::Trigger const irq_trigger,
                           Irq_session::Polarity const irq_polarity)
{
	/* allocate notification object within core's CNode */
	Platform &platform = *platform_specific();
	Range_allocator &phys_alloc = *platform.ram_alloc();

	{
		addr_t       const phys_addr = Untyped_memory::alloc_page(phys_alloc);
		seL4_Untyped const service   = Untyped_memory::untyped_sel(phys_addr).value();

		create<Notification_kobj>(service, platform.core_cnode().sel(),
		                          _kernel_notify_sel);
	}

	/* setup IRQ platform specific */
	long res = _associate(irq_trigger, irq_polarity);
	if (res != seL4_NoError)
		return false;

	seL4_CPtr irq_handler = _kernel_irq_sel.value();
	seL4_CPtr notification = _kernel_notify_sel.value();

	res = seL4_IRQHandler_SetNotification(irq_handler, notification);

	return (res == seL4_NoError);
}


void Irq_object::_wait_for_irq()
{
	seL4_Wait(_kernel_notify_sel.value(), nullptr);
}


void Irq_object::start()
{
	::Thread::start();
	_sync_bootup.lock();
}


void Irq_object::entry()
{
	/* thread is up and ready */
	_sync_bootup.unlock();

	while (true) {

		_wait_for_irq();

		if (!_sig_cap.valid())
			continue;

		notify();
	}
}

void Genode::Irq_object::ack_irq()
{
	int res = seL4_IRQHandler_Ack(_kernel_irq_sel.value());
	if (res != seL4_NoError)
		Genode::error("ack_irq failed - ", res);
}

Irq_object::Irq_object(unsigned irq)
:
	Thread_deprecated<4096>("irq"),
	_sync_bootup(Lock::LOCKED),
	_irq(irq),
	_kernel_irq_sel(platform_specific()->core_sel_alloc().alloc()),
	_kernel_notify_sel(platform_specific()->core_sel_alloc().alloc())
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
		throw Service_denied();

	if (!irq_alloc || irq_alloc->alloc_addr(1, _irq_number).error()) {
		Genode::error("unavailable IRQ ", _irq_number, " requested");
		throw Service_denied();
	}


	Irq_args const irq_args(args);

	if (!_irq_object.associate(irq_args.trigger(), irq_args.polarity())) {
		Genode::error("could not associate with IRQ ", irq_args.irq_number());
		throw Service_denied();
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component()
{
	Genode::error(__PRETTY_FUNCTION__, "- not yet implemented.");
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
	return { .type = Info::Type::INVALID, .address = 0, .value = 0 };
}
