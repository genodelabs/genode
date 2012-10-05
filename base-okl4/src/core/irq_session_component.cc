/*
 * \brief  OKL4-specific implementation of IRQ sessions
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2009-12-15
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <irq_proxy.h>

/* core includes */
#include <irq_root.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/interrupt.h>
#include <l4/security.h>
#include <l4/ipc.h>
} }

using namespace Okl4;
using namespace Genode;


/**
 * Proxy class with generic thread
 */
typedef Irq_proxy<Thread<0x1000> > Proxy;


/* XXX move this functionality to a central place instead of duplicating it */
static inline Okl4::L4_ThreadId_t thread_get_my_global_id()
{
	Okl4::L4_ThreadId_t myself;
	myself.raw = Okl4::__L4_TCR_ThreadWord(UTCB_TCR_THREAD_WORD_MYSELF);
	return myself;
}


/**
 * Platform-specific proxy code
 */
class Irq_proxy_component : public Proxy
{
	protected:

		bool _associate()
		{
			/* allow roottask (ourself) to handle the interrupt */
			L4_LoadMR(0, _irq_number);
			int ret = L4_AllowInterruptControl(L4_rootspace);
			if (ret != 1) {
				PERR("L4_AllowInterruptControl returned %d, error code=%ld\n",
				     ret, L4_ErrorCode());
				return false;
			}

			/* bit to use for IRQ notifications */
			enum { IRQ_NOTIFY_BIT = 13 };

			/*
			 * Note: 'L4_Myself()' does not work for the thread argument of
			 *       'L4_RegisterInterrupt'. We have to specify our global ID.
			 */
			L4_LoadMR(0, _irq_number);
			ret = L4_RegisterInterrupt(thread_get_my_global_id(), IRQ_NOTIFY_BIT, 0, 0);
			if (ret != 1) {
				PERR("L4_RegisterInterrupt returned %d, error code=%ld\n",
				     ret, L4_ErrorCode());
				return false;
			}

			/* prepare ourself to receive asynchronous IRQ notifications */
			L4_Set_NotifyMask(1 << IRQ_NOTIFY_BIT);
			L4_Accept(L4_NotifyMsgAcceptor);

			return true;
		}

		void _wait_for_irq()
		{
			/* wait for asynchronous interrupt notification */
			L4_ThreadId_t partner = L4_nilthread;
			L4_ReplyWait(partner, &partner);
		}

		void _ack_irq()
		{
			L4_LoadMR(0, _irq_number);
			L4_AcknowledgeInterrupt(0, 0);
		}

	public:

		Irq_proxy_component(long irq_number) : Irq_proxy(irq_number)
		{
			_start();
		}
};


/***************************
 ** IRQ session component **
 ***************************/

bool Irq_session_component::Irq_control_component::associate_to_irq(unsigned irq)
{
	return true;
}


void Irq_session_component::wait_for_irq()
{
	/* block at interrupt proxy */
	Proxy *p = Proxy::get_irq_proxy<Irq_proxy_component>(_irq_number);
	if (!p) {
		PERR("Expected to find IRQ proxy for IRQ %02x", _irq_number);
		return;
	}

	p->wait_for_irq();

	/* interrupt ocurred and proxy woke us up */
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc),
	_ep(cap_session, STACK_SIZE, "irqctrl"),
	_irq_attached(false),
	_control_client(Capability<Irq_session_component::Irq_control>())
{
	/*
	 * XXX Removed irq_shared argument as this is the default now. If we need
	 * exclusive later on, we should add this as new argument.
	 */

	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1) {
		PERR("invalid IRQ number requested");

		throw Root::Unavailable();
	}

	/* check if IRQ thread was started before */
	Proxy *irq_proxy = Proxy::get_irq_proxy<Irq_proxy_component>(irq_number, irq_alloc);
	if (!irq_proxy) {
		PERR("unavailable IRQ %lx requested", irq_number);

		throw Root::Unavailable();
	}

	irq_proxy->add_sharer();
	_irq_number = irq_number;

	/* initialize capability */
	_irq_cap = _ep.manage(this);
}


Irq_session_component::~Irq_session_component()
{
	PERR("not yet implemented");
	/* TODO del_sharer() resp. put_sharer() */
}

