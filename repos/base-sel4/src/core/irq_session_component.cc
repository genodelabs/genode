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

	_kernel_notify_phys = Untyped_memory::alloc_page(phys_alloc);

	bool ok = _kernel_notify_phys.convert<bool>([&](auto &phys) {
		auto service = Untyped_memory::untyped_sel(addr_t(phys.ptr)).value();

		return _kernel_notify_sel.convert<bool>([&](auto notify_sel) {
			return create<Notification_kobj>(service,
			                                 platform.core_cnode().sel(),
			                                 Cnode_index(unsigned(notify_sel)));
		}, [&](auto) { return false; });
	}, [&](auto) { return false; });

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

	while (!_stop) {

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


Irq_object::Irq_object(Runtime &runtime, Allocator::Result const &irq)
:
	Thread(runtime, "irq", Stack_size { 4096 }, { }),
	_irq(irq),
	_kernel_irq_sel(platform_specific().core_sel_alloc().alloc()),
	_kernel_notify_sel(platform_specific().core_sel_alloc().alloc())
{ }


void Irq_object::stop_thread()
{
	/* signal IRQ thread to go out-of-service */
	_stop = true;

	_kernel_notify_sel.with_result([&](auto sel) {
		seL4_Signal(sel);
		join();

		with_native_thread([&](auto const &nt) {
			/*
			 * Suppress kernel warnings on thread in destruction ...
			 * since attr.lock_sel may still be in use for sleep_forever.
			 */
			auto ret = seL4_TCB_Suspend(nt.attr.tcb_sel);
			if (ret != seL4_NoError)
				error("could not suspend IRQ thread before destruction");
		}, []() { error("native IRQ thread invalid"); });

	}, [](auto){ error("IRQ thread could not be stopped"); });
}


Irq_object::~Irq_object()
{
	_kernel_irq_sel.with_result([&](auto irq_sel) {
		auto res = seL4_IRQHandler_Clear(irq_sel);
		if (res != seL4_NoError) {
			error("seL4_IRQHandler_Clear failed - leaking resources");
			return;
		}

		res = seL4_CNode_Delete(seL4_CapInitThreadCNode, irq_sel, 32);
		if (res != seL4_NoError) {
			error("~Irq_object: leaking irq_sel");
			return;
		}
		platform_specific().core_sel_alloc().free(Cap_sel(unsigned(irq_sel)));
	}, [](auto) { });

	_kernel_notify_sel.with_result([&](auto sel) {
		auto res = seL4_CNode_Delete(seL4_CapInitThreadCNode, sel, 32);
		if (res != seL4_NoError) {
			error("~Irq_object: leaking notify_sel");
			return;
		}
		platform_specific().core_sel_alloc().free(Cap_sel(unsigned(sel)));
	}, [](auto) { });

	_kernel_notify_phys.with_result([&](auto &phys) {
		auto &platform   = platform_specific();
		auto &phys_alloc = platform.ram_alloc();
		Untyped_memory::free_page(phys_alloc, addr_t(phys.ptr));
	}, [](auto) { });
}


static Allocator::Result allocate_irq(Range_allocator &irq_alloc,
                                      Irq_args const  &args)
{
	if (!args.msi())
		return irq_alloc.alloc_addr(1, args.irq_number());

	/* see contrib include/plat/pc99/plat/machine.h - max user vector is 155 */
	auto const range_msi = Range_allocator::Range(Irq_object::MSI_OFFSET, 155);

	return irq_alloc.alloc_aligned(1 /* size */, 0 /* alignment */, range_msi);
}


Irq_session_component::Irq_session_component(Runtime         &runtime,
                                             Range_allocator &irq_alloc,
                                             const char      *args)
:
	_irq_number(allocate_irq(irq_alloc, Irq_args(args))),
	_irq_object(runtime, _irq_number)
{
	Irq_args irq_args { args };

	if (_irq_number.failed()) {
		error("unavailable ", irq_args.msi() ? "MSI" : "GSI",
		      " requested - irq argument: ", irq_args.irq_number());
		return;
	}

	if (!_irq_object.associate(irq_args)) {
		error("could not associate with ", irq_args.msi() ? "MSI" : "GSI",
		      " - irq argument: ", irq_args.irq_number());
		return;
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component()
{
	_irq_object.stop_thread();
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

	return _irq_number.convert<Info>(
		[&] (Range_allocator::Allocation const &a) -> Info {
			return { .type    = Info::Type::MSI,
			         .address = 0xfee00000ul,
			         .value   = Irq_object::PIC_IRQ_LINES  +
			                    Irq_object::IRQ_INT_OFFSET +
			                    addr_t(a.ptr) };
		}, [&] (Alloc_error) { return Info { }; });
}
