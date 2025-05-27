/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <irq_root.h>
#include <irq_args.h>

/* sel4 includes */
#include <sel4/sel4.h>

using namespace Core;


bool Irq_object::associate(Irq_args const &args)
{
	/* allocate notification object within core's CNode */
	auto &platform   = platform_specific();
	auto &phys_alloc = platform.ram_alloc();

	auto phys_result = Untyped_memory::alloc_page(phys_alloc);

	if (phys_result.failed())
		return false;

	bool ok = false;

	phys_result.with_result([&](auto &phys) {
		auto service = Untyped_memory::untyped_sel(addr_t(phys.ptr)).value();

		_kernel_notify_sel.with_result([&](auto notify_sel) {
			ok = create<Notification_kobj>(service,
			                               platform.core_cnode().sel(),
			                               Cnode_index(unsigned(notify_sel)));
		}, [&](auto) { });

		phys.deallocate = !ok;
	}, [&](auto) { });

	if (!ok)
		return false;

	/* setup IRQ platform specific */
	auto res = _associate(args);
	if (res != seL4_NoError)
		return false;

	_kernel_irq_sel.with_result([&](auto irq_sel) {
		_kernel_notify_sel.with_result([&](auto notify_sel) {
			res = seL4_IRQHandler_SetNotification(irq_sel, notify_sel);
		}, [&](auto) { res = seL4_FailedLookup; });
	}, [&](auto) { res = seL4_FailedLookup; });

	return (res == seL4_NoError);
}


void Irq_object::_wait_for_irq()
{
	_kernel_notify_sel.with_result([&](auto notify_sel) {
		seL4_Wait(notify_sel, nullptr);
	}, [](auto) { error("_wait_for_irq failed"); });
}


Thread::Start_result Irq_object::start()
{
	Thread::Start_result const result = ::Thread::start();
	_sync_bootup.block();
	return result;
}


void Irq_object::entry()
{
	/* thread is up and ready */
	_sync_bootup.wakeup();

	while (true) {

		_wait_for_irq();

		if (!_sig_cap.valid())
			continue;

		notify();
	}
}


void Irq_object::ack_irq()
{
	_kernel_irq_sel.with_result([&](auto sel) {
		int res = seL4_IRQHandler_Ack(sel);
		if (res != seL4_NoError)
			error("ack_irq failed - ", res);
	}, [](auto) { error("irq sel invalid"); });
}


Irq_object::Irq_object(unsigned irq)
:
	Thread(Weight::DEFAULT_WEIGHT, "irq", 4096 /* stack */, Type::NORMAL),
	_irq(irq),
	_kernel_irq_sel(platform_specific().core_sel_alloc().alloc()),
	_kernel_notify_sel(platform_specific().core_sel_alloc().alloc())
{ }


Irq_object::~Irq_object()
{
	_kernel_irq_sel.with_result([&](auto sel) {
		platform_specific().core_sel_alloc().free(Cap_sel(unsigned(sel)));
	}, [](auto) { });

	_kernel_notify_sel.with_result([&](auto sel) {
		platform_specific().core_sel_alloc().free(Cap_sel(unsigned(sel)));
	}, [](auto) { });
}


static unsigned irq_number(Irq_args const &args)
{
	return args.msi() ? unsigned(args.irq_number() + Irq_object::MSI_OFFSET)
	                  : unsigned(args.irq_number());
}


Irq_session_component::Irq_session_component(Range_allocator &irq_alloc,
                                             const char      *args)
:
	_irq_number(irq_alloc.alloc_addr(1, irq_number(Irq_args(args)))),
	_irq_object(irq_number(Irq_args(args)))
{
	if (_irq_number.failed()) {
		error("unavailable interrupt ", Irq_args(args).irq_number(), " requested");
		return;
	}

	if (!_irq_object.associate(Irq_args(args))) {
		error("could not associate with IRQ ", Irq_args(args).irq_number());
		return;
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component()
{
	error(__PRETTY_FUNCTION__, "- not yet implemented.");
}


void Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}


void Irq_session_component::sigh(Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Irq_session::Info Irq_session_component::info()
{
	if (!_irq_object.msi())
		return { };

	// see include/plat/pc99/plat/machine.h
	enum { PIC_IRQ_LINES = 16, IRQ_INT_OFFSET = 0x20 };

	return _irq_number.convert<Info>(
		[&] (Range_allocator::Allocation const &a) -> Info {
			return { .type    = Info::Type::MSI,
			         .address = 0xfee00000ul,
			         .value   = IRQ_INT_OFFSET + PIC_IRQ_LINES + addr_t(a.ptr) };
		}, [&] (Alloc_error) { return Info { }; });
}
