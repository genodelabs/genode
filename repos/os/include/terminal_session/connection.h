/*
 * \brief  Connection to Terminal service
 * \author Norman Feske
 * \date   2011-08-12
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TERMINAL_SESSION__CONNECTION_H_
#define _INCLUDE__TERMINAL_SESSION__CONNECTION_H_

#include <terminal_session/client.h>
#include <base/connection.h>

namespace Terminal { struct Connection; }


struct Terminal::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Wait for connection-established signal
	 *
	 * \noapi
	 */
	static void wait_for_connection(Genode::Capability<Session> cap)
	{
		using namespace Genode;

		/* create signal receiver, just for the single signal */
		Signal_context            sig_ctx;
		Signal_receiver           sig_rec;
		Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

		/* register signal handler */
		cap.call<Rpc_connected_sigh>(sig_cap);

		/* wati for signal */
		sig_rec.wait_for_signal();
		sig_rec.dissolve(&sig_ctx);
	}

	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, char const *label = "")
	:
		Genode::Connection<Session>(env, session(env.parent(),
		                                         "ram_quota=%zd, label=\"%s\"",
		                                         2*4096, label)),
		Session_client(cap())
	{
		wait_for_connection(cap());
	}

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection(char const *label = "")
	:
		Genode::Connection<Session>(session("ram_quota=%zd, label=\"%s\"",
		                                    2*4096, label)),
		Session_client(cap())
	{
		wait_for_connection(cap());
	}
};

#endif /* _INCLUDE__TERMINAL_SESSION__CONNECTION_H_ */
