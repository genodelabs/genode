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
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_
#define _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_

#include <base/rpc_client.h>
#include <signal_session/nova_source.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

namespace Genode {

	class Signal_source_client : public Rpc_client<Nova_signal_source>
	{
		private:

			/**
			 * Capability referring to a NOVA semaphore
			 */
			Native_capability _sem;

			/**
			 * Request NOVA semaphore from signal-source server
			 */
			void _init_sem()
			{
				/* initialize semaphore only once */
				if (_sem.valid()) return;

				/* request mapping of semaphore capability selector */
				_sem = call<Rpc_request_semaphore>();
			}

		public:

			/**
			 * Constructor
			 */
			Signal_source_client(Signal_source_capability cap)
			: Rpc_client<Nova_signal_source>(
				static_cap_cast<Nova_signal_source>(cap)) { }


			/*****************************
			 ** Signal source interface **
			 *****************************/

			Signal wait_for_signal()
			{
				/*
				 * Make sure that we have acquired the
				 * semaphore from the server
				 */
				_init_sem();

				/* 
				 * Block on semaphore, will be unblocked if
				 * signal is available
				 */
				if (Nova::sm_ctrl(_sem.local_name(),
				                  Nova::SEMAPHORE_DOWN))
					nova_die();

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
