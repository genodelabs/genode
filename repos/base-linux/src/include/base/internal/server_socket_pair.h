/*
 * \brief  Socket pair used by RPC entrypoint
 * \author Norman Feske
 * \date   2016-03-25
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__SERVER_SOCKET_PAIR_H_
#define _INCLUDE__BASE__INTERNAL__SERVER_SOCKET_PAIR_H_

namespace Genode {

	struct Socket_pair
	{
		int client_sd = -1;
		int server_sd = -1;
	};

	/*
	 * Helper for obtaining a bound and connected socket pair
	 *
	 * For core, the implementation is just a wrapper around
	 * 'lx_server_socket_pair()'. For all other processes, the implementation
	 * requests the socket pair from the Env::CPU session interface using a
	 * Linux-specific interface extension.
	 */
	Socket_pair server_socket_pair();

	/*
	 * Helper to destroy the server socket pair
	 *
	 * For core, this is a no-op. For all other processes, the server and client
	 * sockets are closed.
	 */
	void destroy_server_socket_pair(Socket_pair);
}

#endif /* _INCLUDE__BASE__INTERNAL__SERVER_SOCKET_PAIR_H_ */
