/*
 * \brief  Connection to UART service
 * \author Norman Feske
 * \date   2012-10-29
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UART_SESSION__CONNECTION_H_
#define _INCLUDE__UART_SESSION__CONNECTION_H_

#include <uart_session/client.h>
#include <terminal_session/connection.h>

namespace Uart { struct Connection; }


struct Uart::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env)
	:
		Genode::Connection<Session>(env, session(env.parent(), "ram_quota=%ld", 2*4096)),
		Session_client(env.rm(), cap())
	{
		Terminal::Connection::wait_for_connection(cap());
	}
};

#endif /* _INCLUDE__UART_SESSION__CONNECTION_H_ */
