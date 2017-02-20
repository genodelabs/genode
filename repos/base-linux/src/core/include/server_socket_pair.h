/*
 * \brief  Support for communication over Unix domain sockets
 * \author Norman Feske
 * \date   2012-08-10
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SERVER_SOCKET_PAIR_H_
#define _CORE__INCLUDE__SERVER_SOCKET_PAIR_H_

/* Linux syscall bindings */
#include <core_linux_syscalls.h>
#include <sys/socket.h>
#include <sys/un.h>

/* base-internal includes */
#include <base/internal/socket_descriptor_registry.h>
#include <base/internal/server_socket_pair.h>

/* core-local includes */
#include <resource_path.h>


/**
 * Utility: Create socket address for server entrypoint at thread ID
 */
struct Uds_addr : sockaddr_un
{
	Uds_addr(long thread_id)
	{
		sun_family = AF_UNIX;
		Genode::snprintf(sun_path, sizeof(sun_path), "%s/ep-%ld",
		                 resource_path(), thread_id);
	}
};


/**
 * Utility: Create named socket pair for given unique ID
 */
static inline Genode::Socket_pair create_server_socket_pair(long id)
{
	Genode::Socket_pair socket_pair;

	/*
	 * Main thread uses 'Ipc_server' for 'sleep_forever()' only. No need for
	 * binding.
	 */
	if (id == -1)
		return socket_pair;

	Uds_addr addr(id);

	/*
	 * Create server-side socket
	 */
	socket_pair.server_sd = lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (socket_pair.server_sd < 0) {
		PRAW("Error: Could not create server-side socket (ret=%d)", socket_pair.server_sd);
		class Server_socket_failed { };
		throw Server_socket_failed();
	}

	/* make sure bind succeeds */
	lx_unlink(addr.sun_path);

	int const bind_ret = lx_bind(socket_pair.server_sd, (sockaddr *)&addr, sizeof(addr));
	if (bind_ret < 0) {
		PRAW("Error: Could not bind server socket (ret=%d)", bind_ret);
		class Bind_failed { };
		throw Bind_failed();
	}

	/*
	 * Create client-side socket
	 */
	socket_pair.client_sd = lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (socket_pair.client_sd < 0) {
		PRAW("Error: Could not create client-side socket (ret=%d)", socket_pair.client_sd);
		class Client_socket_failed { };
		throw Client_socket_failed();
	}

	int const conn_ret = lx_connect(socket_pair.client_sd, (sockaddr *)&addr, sizeof(addr));
	if (conn_ret < 0) {
		PRAW("Error: Could not connect client-side socket (ret=%d)", conn_ret);
		class Connect_failed { };
		throw Connect_failed();
	}

	socket_pair.client_sd = Genode::ep_sd_registry()->try_associate(socket_pair.client_sd, id);

	/*
	 * Wipe Unix domain socket from the file system. It will live as long as
	 * there exist references to it in the form of file descriptors.
	 */
	lx_unlink(addr.sun_path);

	return socket_pair;
}

#endif /* _CORE__INCLUDE__SERVER_SOCKET_PAIR_H_ */
