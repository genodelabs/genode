/*
 * \brief  libc socket operations
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <os/path.h>
#include <util/token.h>

/* libc includes */
#include <errno.h>

/* libc-internal includes */
#include "libc_file.h"
#include "socket_fs_plugin.h"

using namespace Libc;

namespace Libc { extern char const *config_socket(); }


/***********************
 ** Address functions **
 ***********************/

extern "C" int _getpeername(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	if (*Libc::config_socket())
		return socket_fs_getpeername(libc_fd, addr, addrlen);

	FD_FUNC_WRAPPER(getpeername, libc_fd, addr, addrlen);
}


extern "C" int getpeername(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	return _getpeername(libc_fd, addr, addrlen);
}


extern "C" int _getsockname(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	if (*Libc::config_socket())
		return socket_fs_getsockname(libc_fd, addr, addrlen);

	FD_FUNC_WRAPPER(getsockname, libc_fd, addr, addrlen);
}


extern "C" int getsockname(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	return _getsockname(libc_fd, addr, addrlen);
}


/**************************
 ** Socket transport API **
 **************************/

extern "C" int _accept(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	if (*Libc::config_socket())
		return socket_fs_accept(libc_fd, addr, addrlen);

	File_descriptor *ret_fd;
	FD_FUNC_WRAPPER_GENERIC(ret_fd =, 0, accept, libc_fd, addr, addrlen);
	return ret_fd ? ret_fd->libc_fd : INVALID_FD;
}


extern "C" int accept(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	return _accept(libc_fd, addr, addrlen);
}


extern "C" int _bind(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	if (*Libc::config_socket())
		return socket_fs_bind(libc_fd, addr, addrlen);

	FD_FUNC_WRAPPER(bind, libc_fd, addr, addrlen);
}


extern "C" int bind(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	return _bind(libc_fd, addr, addrlen);
}


extern "C" int _connect(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	if (*Libc::config_socket())
		return socket_fs_connect(libc_fd, addr, addrlen);

	FD_FUNC_WRAPPER(connect, libc_fd, addr, addrlen);
}


extern "C" int connect(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
    return _connect(libc_fd, addr, addrlen);
}


extern "C" int _listen(int libc_fd, int backlog)
{
	if (*Libc::config_socket())
		return socket_fs_listen(libc_fd, backlog);

	FD_FUNC_WRAPPER(listen, libc_fd, backlog);
}


extern "C" int listen(int libc_fd, int backlog)
{
	return _listen(libc_fd, backlog);
}


extern "C" ssize_t _recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                             sockaddr *src_addr, socklen_t *src_addrlen)
{
	if (*Libc::config_socket())
		return socket_fs_recvfrom(libc_fd, buf, len, flags, src_addr, src_addrlen);

	FD_FUNC_WRAPPER(recvfrom, libc_fd, buf, len, flags, src_addr, src_addrlen);
}


extern "C" ssize_t recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                            sockaddr *src_addr, socklen_t *src_addrlen)
{
	return _recvfrom(libc_fd, buf, len, flags, src_addr, src_addrlen);
}


extern "C" ssize_t recv(int libc_fd, void *buf, ::size_t len, int flags)
{
	if (*Libc::config_socket())
		return socket_fs_recv(libc_fd, buf, len, flags);

	FD_FUNC_WRAPPER(recv, libc_fd, buf, len, flags);
}


extern "C" ssize_t _recvmsg(int libc_fd, msghdr *msg, int flags)
{
	if (*Libc::config_socket())
		return socket_fs_recvmsg(libc_fd, msg, flags);

	FD_FUNC_WRAPPER(recvmsg, libc_fd, msg, flags);
}


extern "C" ssize_t recvmsg(int libc_fd, msghdr *msg, int flags)
{
	return _recvmsg(libc_fd, msg, flags);
}


extern "C" ssize_t _sendto(int libc_fd, void const *buf, ::size_t len, int flags,
                           sockaddr const *dest_addr, socklen_t dest_addrlen)
{
	if (*Libc::config_socket())
		return socket_fs_sendto(libc_fd, buf, len, flags, dest_addr, dest_addrlen);

	FD_FUNC_WRAPPER(sendto, libc_fd, buf, len, flags, dest_addr, dest_addrlen);
}


extern "C" ssize_t sendto(int libc_fd, void const *buf, ::size_t len, int flags,
                          sockaddr const *dest_addr, socklen_t dest_addrlen)
{
	return _sendto(libc_fd, buf, len, flags, dest_addr, dest_addrlen);
}


extern "C" ssize_t send(int libc_fd, void const *buf, ::size_t len, int flags)
{
	if (*Libc::config_socket())
		return socket_fs_send(libc_fd, buf, len, flags);

	FD_FUNC_WRAPPER(send, libc_fd, buf, len, flags);
}


extern "C" int _getsockopt(int libc_fd, int level, int optname,
                          void *optval, socklen_t *optlen)
{
	if (*Libc::config_socket())
		return socket_fs_getsockopt(libc_fd, level, optname, optval, optlen);

	FD_FUNC_WRAPPER(getsockopt, libc_fd, level, optname, optval, optlen);
}


extern "C" int getsockopt(int libc_fd, int level, int optname,
                          void *optval, socklen_t *optlen)
{
	return _getsockopt(libc_fd, level, optname, optval, optlen);
}


extern "C" int _setsockopt(int libc_fd, int level, int optname,
                           void const *optval, socklen_t optlen)
{
	if (*Libc::config_socket())
		return socket_fs_setsockopt(libc_fd, level, optname, optval, optlen);

	FD_FUNC_WRAPPER(setsockopt, libc_fd, level, optname, optval, optlen);
}


extern "C" int setsockopt(int libc_fd, int level, int optname,
                          void const *optval, socklen_t optlen)
{
	return _setsockopt(libc_fd, level, optname, optval, optlen);
}


extern "C" int _shutdown(int libc_fd, int how)
{
	if (*Libc::config_socket())
		return socket_fs_shutdown(libc_fd, how);

	FD_FUNC_WRAPPER(shutdown, libc_fd, how);
}


extern "C" int shutdown(int libc_fd, int how)
{
	return _shutdown(libc_fd, how);
}


extern "C" int _socket(int domain, int type, int protocol)
{
	if (*Libc::config_socket())
		return socket_fs_socket(domain, type, protocol);

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


extern "C" int socket(int domain, int type, int protocol)
{
	return _socket(domain, type, protocol);
}
