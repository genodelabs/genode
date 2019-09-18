/*
 * \brief  Libc pseudo plugin for socket fs
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2017-02-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__SOCKET_FS_PLUGIN_H_
#define _LIBC__INTERNAL__SOCKET_FS_PLUGIN_H_

/* Libc includes */
#include <sys/types.h>
#include <sys/socket.h>

extern "C" int socket_fs_getpeername(int, sockaddr *, socklen_t *);
extern "C" int socket_fs_getsockname(int, sockaddr *, socklen_t *);
extern "C" int socket_fs_accept(int, sockaddr *, socklen_t *);
extern "C" int socket_fs_bind(int, sockaddr const *, socklen_t);
extern "C" int socket_fs_connect(int, sockaddr const *, socklen_t);
extern "C" int socket_fs_listen(int, int);
extern "C" ssize_t socket_fs_recvfrom(int, void *, ::size_t, int, sockaddr *, socklen_t *);
extern "C" ssize_t socket_fs_recv(int, void *, ::size_t, int);
extern "C" ssize_t socket_fs_recvmsg(int, msghdr *, int);
extern "C" ssize_t socket_fs_sendto(int, void const *, ::size_t, int, sockaddr const *, socklen_t);
extern "C" ssize_t socket_fs_send(int, void const *, ::size_t, int);
extern "C" int socket_fs_getsockopt(int, int, int, void *, socklen_t *);
extern "C" int socket_fs_setsockopt(int, int, int, void const *, socklen_t);
extern "C" int socket_fs_shutdown(int, int);
extern "C" int socket_fs_socket(int, int, int);

#endif /* _LIBC__INTERNAL__SOCKET_FS_PLUGIN_H_ */
