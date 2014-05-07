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

#ifndef _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_
#define _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_

#include <base/rpc_client.h>
#include <base/thread.h>
#include <signal_session/foc_source.h>

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
				                                Thread_base::myself()->tid());
				if (l4_error(tag))
					PERR("l4_irq_attach failed with %ld!", l4_error(tag));
			}

		public:

			/**
			 * Constructor
			 */
			Signal_source_client(Signal_source_capability cap)
			: Rpc_client<Foc_signal_source>(static_cap_cast<Foc_signal_source>(cap))
			{ _init_sem(); }


			/*****************************
			 ** Signal source interface **
			 *****************************/

			/* Build with frame pointer to make GDB backtraces work. See issue #1061. */
			__attribute__((optimize("-fno-omit-frame-pointer")))
			__attribute__((noinline))
			Signal wait_for_signal()
			{
				using namespace Fiasco;

				/* block on semaphore, will be unblocked if signal is available */
				l4_irq_receive(_sem.dst(), L4_IPC_NEVER);

				/*
				 * Now that the server has unblocked the semaphore, we are sure
				 * that there is a signal pending. The following 'wait_for_signal'
				 * request will be immediately answered.
				 */
				return call<Rpc_wait_for_signal>();
			}
	};
}

#endif /* _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_ */
