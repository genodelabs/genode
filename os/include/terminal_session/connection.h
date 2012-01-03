/*
 * \brief  Connection to Terminal service
 * \author Norman Feske
 * \date   2011-08-12
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TERMINAL_SESSION__CONNECTION_H_
#define _INCLUDE__TERMINAL_SESSION__CONNECTION_H_

#include <terminal_session/client.h>
#include <base/connection.h>

#include <base/printf.h>

namespace Terminal {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection()
		:
			Genode::Connection<Session>(session("ram_quota=%zd", 2*4096)),
			Session_client(cap())
		{
			using namespace Genode;

			/*
			 * Wait for connection-established signal
			 */

			/* create signal receiver, just for the single signal */
			Signal_context            sig_ctx;
			Signal_receiver           sig_rec;
			Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

			/* register signal handler */
			call<Rpc_connected_sigh>(sig_cap);

			/* wati for signal */
			sig_rec.wait_for_signal();
			sig_rec.dissolve(&sig_ctx);
		}
	};
}

#endif /* _INCLUDE__TERMINAL_SESSION__CONNECTION_H_ */
