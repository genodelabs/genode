/*
 * \brief  UART session interface
 * \author Norman Feske
 * \date   2012-10-29
 *
 * The UART session interface is an extended Terminal session interface.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UART_SESSION__UART_SESSION_H_
#define _INCLUDE__UART_SESSION__UART_SESSION_H_

/* Genode includes */
#include <terminal_session/terminal_session.h>

namespace Uart {

	using namespace Terminal;

	struct Session;
}


struct Uart::Session : Terminal::Session
{
	static const char *service_name() { return "Uart"; }

	/**
	 * Set baud rate
	 */
	virtual void baud_rate(Genode::size_t bits_per_second) = 0;


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_baud_rate, void, baud_rate, Genode::size_t);
	GENODE_RPC_INTERFACE_INHERIT(Terminal::Session, Rpc_baud_rate);
};

#endif /* _INCLUDE__UART_SESSION__UART_SESSION_H_ */
