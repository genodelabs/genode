/*
 * \brief  libc file operations
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>

/* Genode-specific libc interfaces */
#include <libc-plugin/fd_alloc.h>
#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>

/* libc includes */
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/* libc-internal includes */
#include "libc_mem_alloc.h"

using namespace Libc;


#ifdef GENODE_RELEASE
#undef PERR
#define PERR(...)
#endif /* GENODE_RELEASE */


enum { INVALID_FD = -1 };


/***************
 ** Utilities **
 ***************/

/**
 * Find plugin responsible for the specified libc file descriptor
 *
 * \param func_name  function name of the caller for printing an error message
 */
inline File_descriptor *libc_fd_to_fd(int libc_fd, const char *func_name)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd)
		PERR("no plugin found for %s(%d)", func_name, libc_fd);

	return fd;
}


/**
 * Generate body of wrapper function taking a file descriptor as first argument
 */
#define FD_FUNC_WRAPPER_GENERIC(result_stm, func_name, libc_fd, ...)	\
	File_descriptor *fd = libc_fd_to_fd(libc_fd, #func_name);	\
	if (!fd || !fd->plugin)						\
		result_stm INVALID_FD;					\
	else								\
		result_stm fd->plugin->func_name(fd, ##__VA_ARGS__ );

#define FD_FUNC_WRAPPER(func_name, libc_fd, ...) \
	FD_FUNC_WRAPPER_GENERIC(return, func_name, libc_fd, ##__VA_ARGS__ )

/**
 * Generate body of wrapper function taking a path name as first argument
 */
#define FNAME_FUNC_WRAPPER(func_name, path, ...) \
	Plugin *plugin  = plugin_registry()->get_plugin_for_##func_name(path, ##__VA_ARGS__); \
	if (!plugin) { \
		PERR("no plugin found for %s(\"%s\")", #func_name, path); \
		return -1; \
	} \
	return plugin->func_name(path, ##__VA_ARGS__);


/********************
 ** Libc functions **
 ********************/

extern "C" int _accept(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return accept(libc_fd, addr, addrlen);
}


extern "C" int accept(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	File_descriptor *fd = libc_fd_to_fd(libc_fd, "accept");
	File_descriptor *ret_fd = (fd && fd->plugin) ? fd->plugin->accept(fd, addr, addrlen) : 0;
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


extern "C" int chdir(const char *path) {
	FNAME_FUNC_WRAPPER(chdir, path) }


extern "C" int _close(int libc_fd) {
	FD_FUNC_WRAPPER(close, libc_fd); }


extern "C" int close(int libc_fd) { return _close(libc_fd); }


extern "C" int connect(int libc_fd, const struct sockaddr *addr,
                       socklen_t addrlen) {
	FD_FUNC_WRAPPER(connect, libc_fd, addr, addrlen); }


extern "C" int _connect(int libc_fd, const struct sockaddr *addr,
                        socklen_t addrlen)
{
    return connect(libc_fd, addr, addrlen);
}


extern "C" int _dup2(int libc_fd, int new_libc_fd)
{

	File_descriptor *fd = libc_fd_to_fd(libc_fd, "dup2");
	if (!fd || !fd->plugin)
		return INVALID_FD;

	/*
	 * Check if 'new_libc_fd' is already in use. If so, close it before
	 * allocating it again.
	 */
	File_descriptor *new_fd = file_descriptor_allocator()->find_by_libc_fd(new_libc_fd);
	if (new_fd)
		close(new_libc_fd);

	new_fd = file_descriptor_allocator()->alloc(fd->plugin, 0,  new_libc_fd);
	/* new_fd->context must be assigned by the plugin implementing 'dup2' */
	return fd->plugin->dup2(fd, new_fd);
}


extern "C" int dup2(int libc_fd, int new_libc_fd)
{
	return _dup2(libc_fd, new_libc_fd);
}


extern "C" int fchdir(int libc_fd) {
	FD_FUNC_WRAPPER(fchdir, libc_fd); }


extern "C" int fcntl(int libc_fd, int cmd, ...)
{
	va_list ap;
	int res;
	va_start(ap, cmd);
	FD_FUNC_WRAPPER_GENERIC(res =, fcntl, libc_fd, cmd, va_arg(ap, long));
	va_end(ap);
	return res;
}


extern "C" int _fcntl(int libc_fd, int cmd, long arg) {
	return fcntl(libc_fd, cmd, arg); }


extern "C" void freeaddrinfo(struct addrinfo *res)
{
	Plugin *plugin;

	plugin = plugin_registry()->get_plugin_for_freeaddrinfo(res);

	if (!plugin) {
		PERR("no plugin found for freeaddrinfo()");
		return;
	}

	plugin->freeaddrinfo(res);
}


extern "C" int _fstat(int libc_fd, struct stat *buf) {
	FD_FUNC_WRAPPER(fstat, libc_fd, buf); }


extern "C" int fstat(int libc_fd, struct stat *buf)
{
	return _fstat(libc_fd, buf);
}


extern "C" int _fstatfs(int libc_fd, struct statfs *buf) {
	FD_FUNC_WRAPPER(fstatfs, libc_fd, buf); }


extern "C" int fsync(int libc_fd) {
	FD_FUNC_WRAPPER(fsync, libc_fd); }


extern "C" int getaddrinfo(const char *node, const char *service,
                           const struct addrinfo *hints,
                           struct addrinfo **res)
{
	Plugin *plugin;

	plugin = plugin_registry()->get_plugin_for_getaddrinfo(node, service, hints, res);

	if (!plugin) {
		PERR("no plugin found for getaddrinfo()");
		return -1;
	}

	return plugin->getaddrinfo(node, service, hints, res);
}


extern "C" ssize_t _getdirentries(int libc_fd, char *buf, ::size_t nbytes, ::off_t *basep) {
	FD_FUNC_WRAPPER(getdirentries, libc_fd, buf, nbytes, basep); }



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


extern "C" int ioctl(int libc_fd, int request, char *argp) {
	FD_FUNC_WRAPPER(ioctl, libc_fd, request, argp); }


extern "C" int _ioctl(int libc_fd, int request, char *argp) {
	FD_FUNC_WRAPPER(ioctl, libc_fd, request, argp); }


extern "C" int _listen(int libc_fd, int backlog)
{
	return listen(libc_fd, backlog);
}


extern "C" int listen(int libc_fd, int backlog) {
	FD_FUNC_WRAPPER(listen, libc_fd, backlog); }


extern "C" ::off_t lseek(int libc_fd, ::off_t offset, int whence) {
	FD_FUNC_WRAPPER(lseek, libc_fd, offset, whence); }


extern "C" int mkdir(const char *path, mode_t mode) {
	FNAME_FUNC_WRAPPER(mkdir, path, mode) }


extern "C" void *mmap(void *addr, ::size_t length, int prot, int flags,
                      int libc_fd, ::off_t offset)
{
	/* handle requests for anonymous memory */
	if (!addr && libc_fd == -1)
		return Libc::mem_alloc()->alloc(length, PAGE_SHIFT);

	/* lookup plugin responsible for file descriptor */
	File_descriptor *fd = libc_fd_to_fd(libc_fd, "mmap");
	if (!fd || !fd->plugin) {
		PWRN("mmap not supported for file descriptor %d", libc_fd);
		return (void *)INVALID_FD;
	}

	return fd->plugin->mmap(addr, length, prot, flags, fd, offset);
}


extern "C" int _open(const char *pathname, int flags, ::mode_t mode)
{
	Plugin *plugin;
	File_descriptor *new_fdo;

	plugin = plugin_registry()->get_plugin_for_open(pathname, flags);

	if (!plugin) {
		PERR("no plugin found for open(\"%s\", int)", pathname, flags);
		return -1;
	}

	new_fdo = plugin->open(pathname, flags);
	if (!new_fdo) {
		PERR("plugin()->open(\"%s\") failed", pathname);
		return -1;
	}

	return new_fdo->libc_fd;
}


extern "C" int open(const char *pathname, int flags, ...)
{
	va_list ap;
	va_start(ap, flags);
	int res = _open(pathname, flags, va_arg(ap, unsigned));
	va_end(ap);
	return res;
}


extern "C" int pipe(int pipefd[2])
{
	Plugin *plugin;
	File_descriptor *pipefdo[2];

	plugin = plugin_registry()->get_plugin_for_pipe();

	if (!plugin) {
		PERR("no plugin found for pipe()");
		return -1;
	}

	if (plugin->pipe(pipefdo) == -1) {
		PERR("plugin()->pipe() failed");
		return -1;
	}

	pipefd[0] = pipefdo[0]->libc_fd;
	pipefd[1] = pipefdo[1]->libc_fd;

	return 0;
}


extern "C" ssize_t _read(int libc_fd, void *buf, ::size_t count) {
	FD_FUNC_WRAPPER(read, libc_fd, buf, count); }


extern "C" ssize_t read(int libc_fd, void *buf, ::size_t count)
{
	return _read(libc_fd, buf, count);
}


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


extern "C" int rename(const char *oldpath, const char *newpath) {
	FNAME_FUNC_WRAPPER(rename, oldpath, newpath); }


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
		PERR("no plugin found for socket()");
		return -1;
	}

	new_fdo = plugin->socket(domain, type, protocol);
	if (!new_fdo) {
		PERR("plugin()->socket() failed");
		return -1;
	}

	return new_fdo->libc_fd;
}


extern "C" int _socket(int domain, int type, int protocol)
{
	return socket(domain, type, protocol);
}


extern "C" int stat(const char *path, struct stat *buf) {
	FNAME_FUNC_WRAPPER(stat, path, buf) }


extern "C" int unlink(const char *path) {
	FNAME_FUNC_WRAPPER(unlink, path) }


extern "C" ssize_t _write(int libc_fd, const void *buf, ::size_t count) {
	FD_FUNC_WRAPPER(write, libc_fd, buf, count); }

extern "C" ssize_t write(int libc_fd, const void *buf, ::size_t count) {
	return _write(libc_fd, buf, count); }
