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
extern "C" {
#include <sys/wait.h>
#include <libc_private.h>
}

/* libc-internal includes */
#include <internal/file.h>
#include <internal/socket_fs_plugin.h>
#include <internal/errno.h>


using namespace Libc;


#define __SYS_(ret_type, name, args, body) \
	extern "C" {\
	ret_type  __sys_##name args body \
	ret_type       _##name args __attribute__((alias("__sys_" #name))); \
	ret_type          name args __attribute__((alias("__sys_" #name))); \
	} \

namespace Libc { extern char const *config_socket(); }


/***********************
 ** Address functions **
 ***********************/

extern "C" int getpeername(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	if (*config_socket())
		return socket_fs_getpeername(libc_fd, addr, addrlen);

	return Libc::Errno(ENOTSOCK);
}


extern "C" __attribute__((alias("getpeername")))
int _getpeername(int libc_fd, sockaddr *addr, socklen_t *addrlen);


extern "C" int getsockname(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	if (*config_socket())
		return socket_fs_getsockname(libc_fd, addr, addrlen);

	FD_FUNC_WRAPPER(getsockname, libc_fd, addr, addrlen);
}


extern "C" __attribute__((alias("getsockname")))
int _getsockname(int libc_fd, sockaddr *addr, socklen_t *addrlen);


/**************************
 ** Socket transport API **
 **************************/

__SYS_(int, accept, (int libc_fd, sockaddr *addr, socklen_t *addrlen),
{
	if (*config_socket())
		return socket_fs_accept(libc_fd, addr, addrlen);

	File_descriptor *ret_fd;
	FD_FUNC_WRAPPER_GENERIC(ret_fd =, 0, accept, libc_fd, addr, addrlen);
	return ret_fd ? ret_fd->libc_fd : INVALID_FD;
})


__SYS_(int, accept4, (int libc_fd, struct sockaddr *addr, socklen_t *addrlen, int /*flags*/),
{
	return accept(libc_fd, addr, addrlen);
})


extern "C" int bind(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	if (*config_socket())
		return socket_fs_bind(libc_fd, addr, addrlen);

	FD_FUNC_WRAPPER(bind, libc_fd, addr, addrlen);
}


extern "C" __attribute__((alias("bind")))
int _bind(int libc_fd, sockaddr const *addr, socklen_t addrlen);


__SYS_(int, connect, (int libc_fd, sockaddr const *addr, socklen_t addrlen),
{
	if (*config_socket())
		return socket_fs_connect(libc_fd, addr, addrlen);

	FD_FUNC_WRAPPER(connect, libc_fd, addr, addrlen);
})


extern "C" int listen(int libc_fd, int backlog)
{
	if (*config_socket())
		return socket_fs_listen(libc_fd, backlog);

	FD_FUNC_WRAPPER(listen, libc_fd, backlog);
}


__SYS_(ssize_t, recvfrom, (int libc_fd, void *buf, ::size_t len, int flags,
                           sockaddr *src_addr, socklen_t *src_addrlen),
{
	if (*config_socket())
		return socket_fs_recvfrom(libc_fd, buf, len, flags, src_addr, src_addrlen);

	FD_FUNC_WRAPPER(recvfrom, libc_fd, buf, len, flags, src_addr, src_addrlen);
})


__SYS_(ssize_t, recv, (int libc_fd, void *buf, ::size_t len, int flags),
{
	if (*config_socket())
		return socket_fs_recv(libc_fd, buf, len, flags);

	FD_FUNC_WRAPPER(recv, libc_fd, buf, len, flags);
})


__SYS_(ssize_t, recvmsg, (int libc_fd, msghdr *msg, int flags),
{
	if (*config_socket())
		return socket_fs_recvmsg(libc_fd, msg, flags);

	FD_FUNC_WRAPPER(recvmsg, libc_fd, msg, flags);
})


__SYS_(ssize_t, sendto, (int libc_fd, void const *buf, ::size_t len, int flags,
                          sockaddr const *dest_addr, socklen_t dest_addrlen),
{
	if (*config_socket())
		return socket_fs_sendto(libc_fd, buf, len, flags, dest_addr, dest_addrlen);

	FD_FUNC_WRAPPER(sendto, libc_fd, buf, len, flags, dest_addr, dest_addrlen);
})


extern "C" ssize_t send(int libc_fd, void const *buf, ::size_t len, int flags)
{
	if (*config_socket())
		return socket_fs_send(libc_fd, buf, len, flags);

	FD_FUNC_WRAPPER(send, libc_fd, buf, len, flags);
}


extern "C" int getsockopt(int libc_fd, int level, int optname,
                          void *optval, socklen_t *optlen)
{
	if (*config_socket())
		return socket_fs_getsockopt(libc_fd, level, optname, optval, optlen);

	FD_FUNC_WRAPPER(getsockopt, libc_fd, level, optname, optval, optlen);
}


extern "C" __attribute__((alias("getsockopt")))
int _getsockopt(int libc_fd, int level, int optname,
                void *optval, socklen_t *optlen);


extern "C" int setsockopt(int libc_fd, int level, int optname,
                          void const *optval, socklen_t optlen)
{
	if (*config_socket())
		return socket_fs_setsockopt(libc_fd, level, optname, optval, optlen);

	FD_FUNC_WRAPPER(setsockopt, libc_fd, level, optname, optval, optlen);
}


extern "C" __attribute__((alias("setsockopt")))
int _setsockopt(int libc_fd, int level, int optname,
                void const *optval, socklen_t optlen);


extern "C" int shutdown(int libc_fd, int how)
{
	if (*config_socket())
		return socket_fs_shutdown(libc_fd, how);

	FD_FUNC_WRAPPER(shutdown, libc_fd, how);
}

__SYS_(int, socket, (int domain, int type, int protocol),
{
	if (*config_socket())
		return socket_fs_socket(domain, type, protocol);

	Plugin *plugin;
	File_descriptor *new_fdo;

	plugin = plugin_registry()->get_plugin_for_socket(domain, type, protocol);

	if (!plugin) {
		error("no plugin found for socket()");
		return -1;
	}

	new_fdo = plugin->socket(domain, type, protocol);
	if (!new_fdo) {
		error("plugin()->socket() failed");
		return -1;
	}

	return new_fdo->libc_fd;
})
