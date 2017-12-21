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
#include <base/log.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <util.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/ipc.h>
}

using namespace Genode;
using namespace Pistachio;


static inline L4_ThreadId_t irqno_to_threadid(unsigned int irqno)
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


void Irq_object::start()
{
	::Thread::start();
	_sync_bootup.lock();
}


void Irq_object::entry()
{
	if (!_associate()) {
		error("could not associate with IRQ ", Hex(_irq));
		return;
	}

	/* thread is up and ready */
	_sync_bootup.unlock();

	/* wait for first ack_irq */
	_sync_ack.lock();

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
		_sync_ack.lock();
	}

	while (true) {

		_wait_for_irq();

		if (!_sig_cap.valid())
			continue;

		Genode::Signal_transmitter(_sig_cap).submit(1);

		_sync_ack.lock();
	}
}


Irq_object::Irq_object(unsigned irq)
:
	Thread_deprecated<4096>("irq"),
	_sync_ack(Lock::LOCKED), _sync_bootup(Lock::LOCKED),
	_irq(irq)
{ }


/***************************
 ** IRQ session component **
 ***************************/


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
		error("unavailable IRQ ", Hex(_irq_number), " requested");
		throw Service_denied();
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component()
{
	L4_Word_t res = L4_DeassociateInterrupt(irqno_to_threadid(_irq_number));

	if (res != 1)
		error("L4_DeassociateInterrupt failed");
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
