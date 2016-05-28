/*
 * \brief  Fiasco.OC-specific signal-source client interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-02-03
 *
 * On Fiasco.OC, the signal source server does not provide a blocking
 * 'wait_for_signal' function because this kernel does not support out-of-order
 * IPC replies. Instead, we use an IRQ kernel-object to let the client block
 * until a signal is present at the server.
 *
 * We request the IRQ object capability and attach to the IRQ on construction
 * of the 'Signal_source_client' object.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__CLIENT_H_
#define _INCLUDE__SIGNAL_SOURCE__CLIENT_H_

#include <base/rpc_client.h>
#include <base/thread.h>
#include <signal_source/foc_signal_source.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

namespace Fiasco {
#include <l4/sys/irq.h>
}

namespace Genode {

	class Signal_source_client : public Rpc_client<Foc_signal_source>
	{
		private:

			/**
			 * Capability with 'dst' referring to a Fiasco.OC IRQ object
			 */
			Native_capability _sem;

			/**
			 * Request Fiasco.OC IRQ object from signal-source server
			 */
			void _init_sem()
			{
				using namespace Fiasco;

				/* request mapping of semaphore capability selector */
				_sem = call<Rpc_request_semaphore>();

				l4_msgtag_t tag = l4_irq_attach(_sem.dst(), 0,
				                                Thread::myself()->native_thread().kcap);
				if (l4_error(tag))
					PERR("l4_irq_attach failed with %ld!", l4_error(tag));
			}

		public:

			/**
			 * Constructor
			 */
			Signal_source_client(Capability<Signal_source> cap)
			: Rpc_client<Foc_signal_source>(static_cap_cast<Foc_signal_source>(cap))
			{ _init_sem(); }

			/**
			 * Destructor
			 */
			~Signal_source_client()
			{
				Fiasco::l4_irq_detach(_sem.dst());
			}

			/*****************************
			 ** Signal source interface **
			 *****************************/

			/* Build with frame pointer to make GDB backtraces work. See issue #1061. */
			__attribute__((optimize("-fno-omit-frame-pointer")))
			__attribute__((noinline))
			Signal wait_for_signal()
			{
				using namespace Fiasco;

				Signal signal;
				do {

					/* block on semaphore until signal context was submitted */
					l4_irq_receive(_sem.dst(), L4_IPC_NEVER);

					/*
					 * The following request will return immediately and either
					 * return a valid or a null signal. The latter may happen in
					 * the case a submitted signal context was destroyed (by the
					 * submitter) before we have a chance to raise our request.
					 */
					signal = call<Rpc_wait_for_signal>();

				} while (!signal.imprint());

				return signal;
			}
	};
}

#endif /* _INCLUDE__SIGNAL_SOURCE__CLIENT_H_ */
