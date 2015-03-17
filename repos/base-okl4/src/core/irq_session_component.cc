/*
 * \brief  OKL4-specific implementation of IRQ sessions
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2009-12-15
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_session_component.h>

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


/* XXX move this functionality to a central place instead of duplicating it */
static inline Okl4::L4_ThreadId_t thread_get_my_global_id()
{
	Okl4::L4_ThreadId_t myself;
	myself.raw = Okl4::__L4_TCR_ThreadWord(UTCB_TCR_THREAD_WORD_MYSELF);
	return myself;
}

namespace Genode {
	typedef Irq_proxy<Thread<1024 * sizeof(addr_t)> > Irq_proxy_base;
	class Irq_proxy_component;
}

/**
 * Platform-specific proxy code
 */
class Genode::Irq_proxy_component : public Irq_proxy_base
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

			return true;
		}

		void _wait_for_irq()
		{
			L4_Accept(L4_NotifyMsgAcceptor);

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


void Irq_session_component::ack_irq()
{
	/* block at interrupt proxy */
	if (!_proxy) {
		PERR("Expected to find IRQ proxy for IRQ %02x", _irq_number);
		return;
	}

	_proxy->ack_irq();
}


Irq_session_component::Irq_session_component(Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc)
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
	_proxy = Irq_proxy_component::get_irq_proxy<Irq_proxy_component>(irq_number, irq_alloc);
	if (!_proxy) {
		PERR("unavailable IRQ %lx requested", irq_number);
		throw Root::Unavailable();
	}

	_irq_number = irq_number;
}


Irq_session_component::~Irq_session_component()
{
	if (!_proxy) return;

	if (_irq_sigh.valid())
		_proxy->remove_sharer(&_irq_sigh);
}


void Irq_session_component::sigh(Genode::Signal_context_capability sigh)
{
	if (!_proxy) {
		PERR("signal handler got not registered - irq thread unavailable");
		return;
	}

	Genode::Signal_context_capability old = _irq_sigh;

	if (old.valid() && !sigh.valid())
		_proxy->remove_sharer(&_irq_sigh);

	_irq_sigh = sigh;

	if (!old.valid() && sigh.valid())
		_proxy->add_sharer(&_irq_sigh);
}
