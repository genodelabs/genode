/*
 * \brief  Linux socket utilities
 * \author Christian Helmuth
 * \date   2012-01-17
 *
 * We create two sockets under lx_rpath() for each thread: client and server
 * role. The naming is 'ep-<thread id>-<role>'. The socket descriptors are
 * cached in Thread_base::_tid.
 *
 * Currently two socket files are needed, as the client does not send the reply
 * socket access-rights in a combined message with the payload. In the future,
 * only server sockets must be bound in lx_rpath().
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM__LINUX_SOCKET_H_
#define _PLATFORM__LINUX_SOCKET_H_

/* Genode includes */
#include <base/ipc_generic.h>
#include <base/thread.h>
#include <base/blocking.h>
#include <base/snprintf.h>

#include <linux_rpath.h>
#include <linux_syscalls.h>


extern "C" void wait_for_continue(void);
extern "C" int raw_write_str(const char *str);

#define PRAW(fmt, ...)                                             \
	do {                                                           \
		char str[128];                                             \
		Genode::snprintf(str, sizeof(str),                         \
		                 ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__); \
		raw_write_str(str);                                        \
	} while (0)


/**
 * Utility: Create socket address for thread ID and role (client/server)
 */
static void lx_create_sockaddr(sockaddr_un *addr, long thread_id, char const *role)
{
	addr->sun_family = AF_UNIX;
	Genode::snprintf(addr->sun_path, sizeof(addr->sun_path), "%s/ep-%ld-%s",
	                 lx_rpath(), thread_id, role);
}


/**
 * Utility: Create a socket descriptor and file for given thread and role
 */
static int lx_create_socket(long thread_id, char const *role)
{
	int sd = lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sd < 0) return -1;

	sockaddr_un addr;
	lx_create_sockaddr(&addr, thread_id, role);

	/* make sure bind succeeds */
	lx_unlink(addr.sun_path);

	if (lx_bind(sd, (sockaddr *)&addr, sizeof(addr)) < 0)
		return -2;

	return sd;
}


/**
 * Utility: Unlink socket file and close descriptor
 *
 * XXX Currently, socket destruction is missing. The client socket could be
 *     used from multiple Ipc_client objects. A safe destruction would need
 *     reference counting.
 */
//static void lx_destroy_socket(int sd, long thread_id, char const *role)
//{
//	sockaddr_un addr;
//	lx_create_sockaddr(&addr, thread_id, role);
//
//	lx_unlink(addr.sun_path);
//	lx_close(sd);
//}


/**
 * Get client-socket descriptor for main thread
 */
static int lx_main_client_socket()
{
	static int sd = lx_create_socket(lx_gettid(), "client");

	return sd;
}


/**
 * Utility: Get server socket for given thread
 */
static int lx_server_socket(Genode::Thread_base *thread)
{
	/*
	 * Main thread uses Ipc_server for sleep_forever() only.
	 */
	if (!thread)
		return lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

	if (thread->tid().server == -1)
		thread->tid().server = lx_create_socket(thread->tid().tid, "server");
	return thread->tid().server;
}


/**
 * Utility: Get client socket for given thread
 */
static int lx_client_socket(Genode::Thread_base *thread)
{
	if (!thread) return lx_main_client_socket();

	if (thread->tid().client == -1)
		thread->tid().client = lx_create_socket(thread->tid().tid, "client");
	return thread->tid().client;
}


/**
 * Utility: Send message to thread via given socket descriptor
 */
static void lx_send_to(int sd, long thread_id, char const *target_role,
                       void *msg, Genode::size_t msg_len)
{
	sockaddr_un addr;
	lx_create_sockaddr(&addr, thread_id, target_role);

	int res = lx_sendto(sd, msg, msg_len, 0, (sockaddr *)&addr, sizeof(addr));
	if (res < 0) {
		PRAW("Send error: %d with %s in %d", res, addr.sun_path, lx_gettid());
		wait_for_continue();
		throw Genode::Ipc_error();
	}
}


/**
 * Utility: Receive message via given socket descriptor
 */
static void lx_recv_from(int sd, void *buf, Genode::size_t buf_len)
{
	socklen_t fromlen;
	int res = lx_recvfrom(sd, buf, buf_len, 0, 0, &fromlen);
	if (res < 0) {
		if ((-res) == EINTR)
			throw Genode::Blocking_canceled();
		else {
			PRAW("Recv error: %d in %d", res, lx_gettid());
			wait_for_continue();
			throw Genode::Ipc_error();
		}
	}
}

#endif /* _PLATFORM__LINUX_SOCKET_H_ */
