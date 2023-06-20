/*
 * \brief  NOVA-specific signal-source client interface
 * \author Norman Feske
 * \date   2010-02-03
 *
 * On NOVA, the signal source server does not provide a blocking
 * 'wait_for_signal' function because this kernel does no support
 * out-of-order IPC replies. Instead, we use a shared semaphore
 * to let the client block until a signal is present at the
 * server. The shared semaphore gets initialized with the first
 * call of 'wait_for_signal()'.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__CLIENT_H_
#define _INCLUDE__SIGNAL_SOURCE__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <cpu_session/cpu_session.h>
#include <signal_source/nova_signal_source.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>
#include <nova/capability_space.h>

namespace Genode {

	class Signal_source_client : public Rpc_client<Nova_signal_source>
	{
		private:

			/**
			 * Capability referring to a NOVA semaphore
			 */
			Native_capability _sem { };

		public:

			/**
			 * Constructor
			 */
			Signal_source_client(Cpu_session &, Capability<Signal_source> cap)
			: Rpc_client<Nova_signal_source>(static_cap_cast<Nova_signal_source>(cap))
			{
				/* request mapping of semaphore capability selector */
				Thread * myself = Thread::myself();
				auto const &exc_base = myself->native_thread().exc_pt_sel;
				request_signal_sm_cap(exc_base + Nova::PT_SEL_PAGE_FAULT,
				                      exc_base + Nova::SM_SEL_SIGNAL);
				_sem = Capability_space::import(exc_base + Nova::SM_SEL_SIGNAL);
				call<Rpc_register_semaphore>(_sem);
			}

			~Signal_source_client()
			{
				Nova::revoke(Nova::Obj_crd(_sem.local_name(), 0));
			}


			/*****************************
			 ** Signal source interface **
			 *****************************/

			Signal wait_for_signal() override
			{
				using namespace Nova;

				mword_t imprint, count;
				do {

					/*
					 * We set an invalid imprint (0) to detect a spurious
					 * unblock. In this case, NOVA does not block
					 * SEMAPHORE_DOWN nor touch our input values if the
					 * deblocking (chained) semaphore was dequeued before we
					 * intend to block.
					 */
					imprint = 0;
					count   = 0;

					/* block on semaphore until signal context was submitted */
					if (uint8_t res = si_ctrl(_sem.local_name(), SEMAPHORE_DOWN,
					                  imprint, count))
						warning("signal reception failed - error ", res);

				} while (imprint == 0);

				return Signal(imprint, (int)count);
			}
	};
}

#endif /* _INCLUDE__SIGNAL_SOURCE__CLIENT_H_ */
