/*
 * \brief  Pistachio-specific implementation of IRQ sessions
 * \author Julian Stecklina
 * \date   2008-02-21
 *
 * FIXME ram quota missing
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_proxy.h>
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


namespace Genode {
       typedef Irq_proxy<Thread<1024 * sizeof(addr_t)> > Irq_proxy_base;
       class Irq_proxy_component;
}


/**
 * Platform-specific proxy code
 */

class Genode::Irq_proxy_component : public Irq_proxy_base
{
	private:

		/*
		 * On Pistachio, an IRQ is unmasked right after attaching.
		 * Hence, the kernel may send an IRQ IPC when the IRQ hander is
		 * not explicitly waiting for an IRQ but when it is waiting for
		 * a client's 'wait_for_irq' RPC call. To avoid this conflict, we
		 * lazily associate to the IRQ when calling the 'wait_for_irq'
		 * function for the first time. We use the '_irq_attached' flag
		 * for detecting the first call.
		 */
		bool _irq_attached;  /* true if IRQ is already attached */

	protected:

		bool _associate() { return true; }

		void _wait_for_irq()
		{
			L4_ThreadId_t irq_thread = irqno_to_threadid(_irq_number);

			/* attach to IRQ when called for the first time */
			L4_MsgTag_t res;
			if (!_irq_attached) {

				if (L4_AssociateInterrupt(irq_thread, L4_Myself()) != true) {
					PERR("L4_AssociateInterrupt failed");
					return;
				}

				/*
				 * Right after associating with an interrupt, the interrupt is
				 * unmasked. Hence, we do not need to send an unmask message
				 * to the IRQ thread but just wait for the IRQ.
				 */
				L4_Set_MsgTag(L4_Niltag);
				res = L4_Receive(irq_thread);

				/*
				 * Now, the IRQ is masked. To receive the next IRQ we have to send
				 * an unmask message to the IRQ thread first.
				 */
				_irq_attached = true;

			/* receive subsequent interrupt */
			} else {

				/* send unmask message and wait for new IRQ */
				L4_Set_MsgTag(L4_Niltag);
				res = L4_Call(irq_thread);
			}

			if (L4_IpcFailed(res)) {
				PERR("ipc error while waiting for interrupt.");
				return;
			}
		}

		void _ack_irq() { }

	public:

		Irq_proxy_component(long irq_number)
		:
			Irq_proxy(irq_number),
			_irq_attached(false)
		{
			_start();
		}

		~Irq_proxy_component()
		{
			L4_ThreadId_t const thread_id = irqno_to_threadid(_irq_number);
			L4_Word_t     const       res = L4_DeassociateInterrupt(thread_id);

			if (res != 1)
				PERR("L4_DeassociateInterrupt failed");
		}
};


/***************************
 ** IRQ session component **
 ***************************/


void Irq_session_component::ack_irq()
{
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
