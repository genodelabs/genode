/*
 * \brief  Pistachio-specific implementation of IRQ sessions
 * \author Julian Stecklina
 * \date   2008-02-21
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <irq_args.h>
#include <util.h>

using namespace Core;
using namespace Pistachio;


static inline L4_ThreadId_t irqno_to_threadid(unsigned irqno)
{
	/*
	 * Interrupt threads have their number as thread_no and a version of 1.
	 */
	return L4_GlobalId(irqno, 1);
}


bool Irq_object::_associate()
{
	L4_ThreadId_t const irq_thread = irqno_to_threadid(_irq);

	return L4_AssociateInterrupt(irq_thread, L4_Myself());
}


void Irq_object::_wait_for_irq()
{
	L4_ThreadId_t const irq_thread = irqno_to_threadid(_irq);

	/* send unmask message and wait for new IRQ */
	L4_Set_MsgTag(L4_Niltag);
	L4_MsgTag_t res = L4_Call(irq_thread);

	if (L4_IpcFailed(res))
		error("ipc error while waiting for interrupt");
}


Thread::Start_result Irq_object::start()
{
	Start_result const result = ::Thread::start();
	_sync_bootup.block();
	return result;
}


void Irq_object::entry()
{
	if (!_associate()) {
		error("could not associate with IRQ ", Hex(_irq));
		return;
	}

	/* thread is up and ready */
	_sync_bootup.wakeup();

	/* wait for first ack_irq */
	_sync_ack.block();

	/*
	 * Right after associating with an interrupt, the interrupt is
	 * unmasked. Hence, we do not need to send an unmask message
	 * to the IRQ thread but just wait for the IRQ.
	 */
	L4_ThreadId_t const irq_thread = irqno_to_threadid(_irq);
	L4_Set_MsgTag(L4_Niltag);
	L4_MsgTag_t res = L4_Receive(irq_thread);

	if (L4_IpcFailed(res))
		error("ipc error while attaching to interrupt");

	/*
	 * Now, the IRQ is masked. To receive the next IRQ we have to send
	 * an unmask message to the IRQ thread first.
	 */
	if (_sig_cap.valid()) {
		Genode::Signal_transmitter(_sig_cap).submit(1);
		_sync_ack.block();
	}

	while (true) {

		_wait_for_irq();

		if (!_sig_cap.valid())
			continue;

		Genode::Signal_transmitter(_sig_cap).submit(1);

		_sync_ack.block();
	}
}


Irq_object::Irq_object(unsigned irq)
:
	Thread(Weight::DEFAULT_WEIGHT, "irq", 4096 /* stack */, Type::NORMAL),
	_irq(irq)
{ }


/***************************
 ** IRQ session component **
 ***************************/

static Range_allocator::Result allocate(Range_allocator &irq_alloc, Irq_args const &args)
{
	if (args.msi())
		return Alloc_error::DENIED;

	return irq_alloc.alloc_addr(1, args.irq_number());
}


Irq_session_component::Irq_session_component(Range_allocator &irq_alloc,
                                             const char      *args)
:
	_irq_number(allocate(irq_alloc, Irq_args(args))),
	_irq_object(Irq_args(args).irq_number())
{
	if (_irq_number.failed()) {
		error("unavailable interrupt ", Irq_args(args).irq_number(), " requested");
		return;
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component()
{
	_irq_number.with_result([&] (Range_allocator::Allocation const &a) {
		unsigned const n = unsigned(addr_t(a.ptr));
		if (L4_DeassociateInterrupt(irqno_to_threadid(n)) != 1)
			error("L4_DeassociateInterrupt failed");

	}, [] (Alloc_error) { });
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
