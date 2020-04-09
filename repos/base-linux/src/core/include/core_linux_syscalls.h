/*
 * \brief  Linux system calls that are used in core only
 * \author Norman Feske
 * \date   2012-08-10
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_LINUX_SYSCALLS_H_
#define _CORE__INCLUDE__CORE_LINUX_SYSCALLS_H_

/* basic Linux syscall bindings */
#include <linux_syscalls.h>

#define size_t __SIZE_TYPE__ /* see comment in 'linux_syscalls.h' */
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#undef size_t

#include <sys/ioctl.h>


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


/***********************************************************
 ** Functions used by core's io port session support code **
 ***********************************************************/

inline int lx_ioperm(unsigned long from, unsigned long num, int turn_on)
{
	return lx_syscall(SYS_ioperm, from, num, turn_on);
}

inline int lx_iopl(int level)
{
	return lx_syscall(SYS_iopl, level);
}

/**************************************************
 ** Functions used by core's io mem session code **
 **************************************************/

/* specific ioctl call for /dev/hwio since I don't want to handle variant args */
inline int lx_ioctl_iomem(int fd, unsigned long phys, Genode::size_t offset)
{
	struct {
		unsigned long phys;
		Genode::size_t length;
	} range = {phys, offset};

	return lx_syscall(SYS_ioctl, fd, _IOW('g', 1, void *), &range);
}


inline int lx_ioctl_irq(int fd, int irq)
{
	return lx_syscall(SYS_ioctl, fd, _IOW('g', 2, int*), &irq);
}


/**************************************
 ** Process creation and destruction **
 **************************************/

inline int lx_execve(const char *filename, char *const argv[],
                     char *const envp[])
{
	return lx_syscall(SYS_execve, filename, argv, envp);
}


inline int lx_kill(int pid, int signal)
{
	return lx_syscall(SYS_kill, pid, signal);
}


inline int lx_create_process(int (*entry)(void *), void *stack, void *arg)
{
	/*
	 * The low byte of the flags denotes the signal to be sent to the parent
	 * when the process terminates. We want core to receive SIGCHLD signals on
	 * this condition.
	 */
	int const flags = CLONE_VFORK | LX_SIGCHLD;
	return lx_clone((int (*)(void *))entry, stack, flags, arg);
}


inline int lx_setuid(unsigned int uid)
{
	return lx_syscall(SYS_setuid, uid);
}


inline int lx_setgid(unsigned int gid)
{
	return lx_syscall(SYS_setgid, gid);
}


/**
 * Query PID of any terminated child
 *
 * This function is called be core after having received a SIGCHLD signal to
 * determine the PID of a terminated Genode process.
 *
 * \return  PID of terminated process or -1 if no process was terminated
 */
inline int lx_pollpid()
{
	return lx_syscall(SYS_wait4, -1 /* any PID */, (int *)0, 1 /* WNOHANG */, 0);
}


/**
 * Disable address-space layout randomization for child processes
 *
 * The virtual address space layout is managed by Genode, not the kernel.
 * Otherwise, the libc's fork mechanism could not work on Linux.
 */
inline void lx_disable_aslr()
{
	/* defined in linux/personality.h */
	enum { ADDR_NO_RANDOMIZE = 0x0040000UL };

	unsigned long const orig_flags = lx_syscall(SYS_personality, 0xffffffff);

	(void)lx_syscall(SYS_personality, orig_flags | ADDR_NO_RANDOMIZE);
}


/***********************************
 ** Resource-limit initialization **
 ***********************************/

inline void lx_boost_rlimit()
{
	rlimit rlimit { };

	if (int const res = lx_syscall(SYS_getrlimit, RLIMIT_NOFILE, &rlimit)) {
		Genode::warning("unable to obtain RLIMIT_NOFILE (", res, "), keeping limit unchanged");
		return;
	}

	/* increase soft limit to hard limit */
	rlimit.rlim_cur = rlimit.rlim_max;

	if (int const res = lx_syscall(SYS_setrlimit, RLIMIT_NOFILE, &rlimit))
		Genode::warning("unable to boost RLIMIT_NOFILE (", res, "), keeping limit unchanged");
}


/********************************************
 ** Communication over Unix-domain sockets **
 ********************************************/

#ifdef SYS_socketcall

inline int lx_socket(int domain, int type, int protocol)
{
	long args[3] = { domain, type, protocol };
	return lx_socketcall(SYS_SOCKET, args);
}


inline int lx_bind(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen)
{
	long args[3] = { sockfd, (long)addr, (long)addrlen };
	return lx_socketcall(SYS_BIND, args);
}


inline int lx_connect(int sockfd, const struct sockaddr *serv_addr,
                      socklen_t addrlen)
{
	long args[3] = { sockfd, (long)serv_addr, (long)addrlen };
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


/******************************
 ** Linux signal dispatching **
 ******************************/

inline int lx_pipe(int pipefd[2])
{
	return lx_syscall(SYS_pipe, pipefd);
}


inline int lx_read(int fd, void *buf, Genode::size_t count)
{
	return lx_syscall(SYS_read, fd, buf, count);
}


#endif /* _CORE__INCLUDE__CORE_LINUX_SYSCALLS_H_ */
