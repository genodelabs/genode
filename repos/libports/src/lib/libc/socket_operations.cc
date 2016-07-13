/*
 * \brief  libc socket operations
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <os/path.h>
#include <util/token.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

/* libc-internal includes */
#include "libc_file.h"

using namespace Libc;


extern "C" int _accept(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return accept(libc_fd, addr, addrlen);
}


extern "C" int accept(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	File_descriptor *ret_fd;
	FD_FUNC_WRAPPER_GENERIC(ret_fd =, 0, accept, libc_fd, addr, addrlen);
	return ret_fd ? ret_fd->libc_fd : INVALID_FD;
}


extern "C" int _bind(int libc_fd, const struct sockaddr *addr,
                     socklen_t addrlen)
{
	return bind(libc_fd, addr, addrlen);
}


extern "C" int bind(int libc_fd, const struct sockaddr *addr,
                    socklen_t addrlen) {
	FD_FUNC_WRAPPER(bind, libc_fd, addr, addrlen); }


extern "C" int connect(int libc_fd, const struct sockaddr *addr,
                       socklen_t addrlen) {
	FD_FUNC_WRAPPER(connect, libc_fd, addr, addrlen); }


extern "C" int _connect(int libc_fd, const struct sockaddr *addr,
                        socklen_t addrlen)
{
    return connect(libc_fd, addr, addrlen);
}


extern "C" void freeaddrinfo(struct addrinfo *res)
{
	Plugin *plugin;

	plugin = plugin_registry()->get_plugin_for_freeaddrinfo(res);

	if (!plugin) {
		Genode::error("no plugin found for freeaddrinfo()");
		return;
	}

	plugin->freeaddrinfo(res);
}


extern "C" int getaddrinfo(const char *node, const char *service,
                           const struct addrinfo *hints,
                           struct addrinfo **res)
{
	Plugin *plugin;

	plugin = plugin_registry()->get_plugin_for_getaddrinfo(node, service, hints, res);

	if (!plugin) {
		Genode::error("no plugin found for getaddrinfo()");
		return -1;
	}

	return plugin->getaddrinfo(node, service, hints, res);
}


extern "C" int _getpeername(int libc_fd, struct sockaddr *addr, socklen_t *addrlen) {
	FD_FUNC_WRAPPER(getpeername, libc_fd, addr, addrlen); }


extern "C" int getpeername(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return _getpeername(libc_fd, addr, addrlen);
}


extern "C" int _getsockname(int libc_fd, struct sockaddr *addr, socklen_t *addrlen) {
	FD_FUNC_WRAPPER(getsockname, libc_fd, addr, addrlen); }


extern "C" int getsockname(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return _getsockname(libc_fd, addr, addrlen);
}


extern "C" int _listen(int libc_fd, int backlog)
{
	return listen(libc_fd, backlog);
}


extern "C" int listen(int libc_fd, int backlog) {
	FD_FUNC_WRAPPER(listen, libc_fd, backlog); }


extern "C" ssize_t recv(int libc_fd, void *buf, ::size_t len, int flags) {
	FD_FUNC_WRAPPER(recv, libc_fd, buf, len, flags); }


extern "C" ssize_t _recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                              struct sockaddr *src_addr, socklen_t *addrlen) {
	FD_FUNC_WRAPPER(recvfrom, libc_fd, buf, len, flags, src_addr, addrlen); }


extern "C" ssize_t recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                              struct sockaddr *src_addr, socklen_t *addrlen)
{
	return _recvfrom(libc_fd, buf, len, flags, src_addr, addrlen);
}


extern "C" ssize_t recvmsg(int libc_fd, struct msghdr *msg, int flags) {
	FD_FUNC_WRAPPER(recvmsg, libc_fd, msg, flags); }


extern "C" ssize_t send(int libc_fd, const void *buf, ::size_t len, int flags) {
	FD_FUNC_WRAPPER(send, libc_fd, buf, len, flags); }


extern "C" ssize_t _sendto(int libc_fd, const void *buf, ::size_t len, int flags,
                            const struct sockaddr *dest_addr, socklen_t addrlen) {
	FD_FUNC_WRAPPER(sendto, libc_fd, buf, len, flags, dest_addr, addrlen); }


extern "C" ssize_t sendto(int libc_fd, const void *buf, ::size_t len, int flags,
                            const struct sockaddr *dest_addr, socklen_t addrlen)
{
	return _sendto(libc_fd, buf, len, flags, dest_addr, addrlen);
}


extern "C" int _getsockopt(int libc_fd, int level, int optname,
                          void *optval, socklen_t *optlen)
{
	return getsockopt(libc_fd, level, optname, optval, optlen);
}


extern "C" int getsockopt(int libc_fd, int level, int optname,
                          void *optval, socklen_t *optlen) {
	FD_FUNC_WRAPPER(getsockopt, libc_fd, level, optname, optval, optlen); }


extern "C" int _setsockopt(int libc_fd, int level, int optname,
                          const void *optval, socklen_t optlen) {
	FD_FUNC_WRAPPER(setsockopt, libc_fd, level, optname, optval, optlen); }


extern "C" int setsockopt(int libc_fd, int level, int optname,
                          const void *optval, socklen_t optlen)
{
	return _setsockopt(libc_fd, level, optname, optval, optlen);
}


extern "C" int shutdown(int libc_fd, int how) {
	FD_FUNC_WRAPPER(shutdown, libc_fd, how); }


extern "C" int socket(int domain, int type, int protocol)
{
	Plugin *plugin;
	File_descriptor *new_fdo;

	plugin = plugin_registry()->get_plugin_for_socket(domain, type, protocol);

	if (!plugin) {
		Genode::error("no plugin found for socket()");
		return -1;
	}

	new_fdo = plugin->socket(domain, type, protocol);
	if (!new_fdo) {
		Genode::error("plugin()->socket() failed");
		return -1;
	}

	return new_fdo->libc_fd;
}


extern "C" int _socket(int domain, int type, int protocol)
{
	return socket(domain, type, protocol);
}
