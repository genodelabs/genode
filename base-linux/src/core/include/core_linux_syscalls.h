/*
 * \brief  Linux system calls that are used in core only
 * \author Norman Feske
 * \date   2012-08-10
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_LINUX_SYSCALLS_H_
#define _CORE__INCLUDE__CORE_LINUX_SYSCALLS_H_

/* basic Linux syscall bindings */
#include <linux_syscalls.h>
#include <sys/stat.h>


/*******************************************************
 ** Functions used by core's ram-session support code **
 *******************************************************/

inline int lx_mkdir(char const *pathname, mode_t mode)
{
	return lx_syscall(SYS_mkdir, pathname, mode);
}


inline int lx_ftruncate(int fd, unsigned long length)
{
	return lx_syscall(SYS_ftruncate, fd, length);
}


inline int lx_unlink(const char *fname)
{
	return lx_syscall(SYS_unlink, fname);
}


/*******************************************************
 ** Functions used by core's rom-session support code **
 *******************************************************/

inline int lx_open(const char *pathname, int flags, mode_t mode = 0)
{
	return lx_syscall(SYS_open, pathname, flags, mode);
}


inline int lx_stat(const char *path, struct stat64 *buf)
{
#ifdef _LP64
	return lx_syscall(SYS_stat, path, buf);
#else
	return lx_syscall(SYS_stat64, path, buf);
#endif
}


/**************************************
 ** Process creation and destruction **
 **************************************/

/**
 * Send signal to process
 *
 * This function is used by core to kill processes.
 */
inline int lx_kill(int pid, int signal)
{
	return lx_syscall(SYS_kill, pid, signal);
}


/********************************************
 ** Communication over Unix-domain sockets **
 ********************************************/

#ifdef SYS_socketcall

inline int lx_socket(int domain, int type, int protocol)
{
	unsigned long args[3] = { domain, type, protocol };
	return lx_socketcall(SYS_SOCKET, args);
}


inline int lx_bind(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen)
{
	unsigned long args[3] = { sockfd, (unsigned long)addr, addrlen };
	return lx_socketcall(SYS_BIND, args);
}


inline int lx_connect(int sockfd, const struct sockaddr *serv_addr,
                      socklen_t addrlen)
{
	unsigned long args[3] = { sockfd, (unsigned long)serv_addr, addrlen };
	return lx_socketcall(SYS_CONNECT, args);
}

#else

inline int lx_socket(int domain, int type, int protocol)
{
	return lx_syscall(SYS_socket, domain, type, protocol);
}


inline int lx_bind(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen)
{
	return lx_syscall(SYS_bind, sockfd, addr, addrlen);
}


inline int lx_connect(int sockfd, const struct sockaddr *serv_addr,
                      socklen_t addrlen)
{
	return lx_syscall(SYS_connect, sockfd, serv_addr, addrlen);
}

#endif /* SYS_socketcall */


#endif /* _CORE__INCLUDE__CORE_LINUX_SYSCALLS_H_ */
