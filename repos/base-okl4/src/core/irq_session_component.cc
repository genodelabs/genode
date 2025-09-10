/*
 * \brief  OKL4-specific implementation of IRQ sessions
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2009-12-15
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <irq_args.h>
#include <irq_session_component.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/okl4.h>

using namespace Okl4;
using namespace Core;


/* bit to use for IRQ notifications */
enum { IRQ_NOTIFY_BIT = 13 };


/* XXX move this functionality to a central place instead of duplicating it */
static inline Okl4::L4_ThreadId_t thread_get_my_global_id()
{
	Okl4::L4_ThreadId_t myself;
	myself.raw = Okl4::__L4_TCR_ThreadWord(UTCB_TCR_THREAD_WORD_MYSELF);
	return myself;
}


bool Irq_object::_associate()
{
	/* allow roottask (ourself) to handle the interrupt */
	L4_LoadMR(0, _irq);
	int ret = L4_AllowInterruptControl(L4_rootspace);
	if (ret != 1) {
		error("L4_AllowInterruptControl returned ", ret, ", error=",
		      L4_ErrorCode());
		return false;
	}

	/*
	 * Note: 'L4_Myself()' does not work for the thread argument of
	 *       'L4_RegisterInterrupt'. We have to specify our global ID.
	 */
	L4_LoadMR(0, _irq);
	ret = L4_RegisterInterrupt(thread_get_my_global_id(), IRQ_NOTIFY_BIT, 0, 0);
	if (ret != 1) {
		error("L4_RegisterInterrupt returned ", ret, ", error=",
		      L4_ErrorCode());
		return false;
	}

	return true;
}


void Irq_object::_wait_for_irq()
{
	/* prepare ourself to receive asynchronous IRQ notifications */
	L4_Set_NotifyMask(1 << IRQ_NOTIFY_BIT);
	L4_Accept(L4_NotifyMsgAcceptor);

	/* wait for asynchronous interrupt notification */
	L4_ThreadId_t partner = L4_nilthread;
	L4_ReplyWait(partner, &partner);
}


Thread::Start_result Irq_object::start()
{
	Start_result const result = ::Thread::start();
	_sync_bootup.block();
	return result;
}


void Irq_object::entry()
{
	if (!_associate())
		error("could not associate with IRQ ", Hex(_irq));

	/* thread is up and ready */
	_sync_bootup.wakeup();

	/* wait for first ack_irq */
	_sync_ack.block();

	while (true) {

		L4_LoadMR(0, _irq);
		L4_AcknowledgeInterrupt(0, 0);

		_wait_for_irq();

		if (!_sig_cap.valid())
			continue;

		Signal_transmitter(_sig_cap).submit(1);

		_sync_ack.block();
	}
}


Irq_object::Irq_object(Runtime &runtime, unsigned irq)
:
	Thread(runtime, "irq", Stack_size { 4096 }, { }), _irq(irq)
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


Irq_session_component::Irq_session_component(Runtime         &runtime,
                                             Range_allocator &irq_alloc,
                                             const char      *args)
:
	_irq_number(allocate(irq_alloc, Irq_args(args))),
	_irq_object(runtime, Irq_args(args).irq_number())
{
	if (_irq_number.failed()) {
		error("unavailable interrupt ", Irq_args(args).irq_number(), " requested");
		return;
	}

	_irq_object.start();
}


Irq_session_component::~Irq_session_component() { }


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
	/* no MSI support */
	return { .type = Info::Type::INVALID, .address = 0, .value = 0 };
}
