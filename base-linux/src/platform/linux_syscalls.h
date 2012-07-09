/*
 * \brief  Linux system-call wrappers
 * \author Norman Feske
 * \date   2008-10-22
 *
 * This file is meant to be internally used by the framework. It is not public
 * interface.
 *
 * From within the framework libraries, we have to use the Linux syscall
 * interface directly rather than relying on convenient libC functions to be
 * able to link this part of the framework to a custom libC. Otherwise, we
 * would end up with very nasty cyclic dependencies when using framework
 * functions such as IPC from the libC back end.
 *
 * The Linux syscall interface looks different for 32bit and 64bit system, in
 * particular regarding the socket interface. On 32bit systems, all socket
 * operations are invoked via the 'socketcall' syscall. On 64bit systems, the
 * different socket functions have distinct syscalls.
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM__LINUX_SYSCALLS_H_
#define _PLATFORM__LINUX_SYSCALLS_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1 /* needed to enable the definition of 'stat64' */
#endif

/* Linux includes */
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>

/* Genode includes */
#include <util/string.h>


/*****************************************
 ** Functions used by the IPC framework **
 *****************************************/

#include <linux/net.h>

extern "C" long lx_syscall(int number, ...);
extern "C" int  lx_clone(int (*fn)(void *), void *child_stack,
                         int flags, void *arg);


inline Genode::uint16_t lx_bswap16(Genode::uint16_t x)
{
	char v[2] = {
		(x & 0xff00) >>  8,
		(x & 0x00ff) >>  0,
	};
	return *(Genode::uint16_t *)v;
}


inline Genode::uint32_t lx_bswap32(Genode::uint32_t x)
{
	char v[4] = {
		(x & 0xff000000) >> 24,
		(x & 0x00ff0000) >> 16,
		(x & 0x0000ff00) >>  8,
		(x & 0x000000ff) >>  0,
	};
	return *(Genode::uint32_t *)v;
}

#define lx_htonl(x) lx_bswap32(x)
#define lx_htons(x) lx_bswap16(x)
#define lx_ntohs(x) lx_bswap16(x)

#ifdef SYS_socketcall

inline int lx_socketcall(int call, unsigned long *args)
{
	int res = lx_syscall(SYS_socketcall, call, args);
	return res;
}


inline int lx_socket(int domain, int type, int protocol)
{
	unsigned long args[3] = { domain, type, protocol };
	return lx_socketcall(SYS_SOCKET, args);
}


inline int lx_connect(int sockfd, const struct sockaddr *serv_addr,
                      socklen_t addrlen)
{
	unsigned long args[3] = { sockfd, (unsigned long)serv_addr, addrlen };
	return lx_socketcall(SYS_CONNECT, args);
}


inline int lx_bind(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen)
{
	unsigned long args[3] = { sockfd, (unsigned long)addr, addrlen };
	return lx_socketcall(SYS_BIND, args);
}


inline int lx_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
	unsigned long args[3] = { s, (unsigned long)name, (unsigned long)namelen };
	return lx_socketcall(SYS_GETSOCKNAME, args);
}


inline ssize_t lx_recvfrom(int s, void *buf, Genode::size_t len, int flags,
                           struct sockaddr *from, socklen_t *from_len)
{
	unsigned long args[6] = { s, (unsigned long)buf, len, flags,
	                          (unsigned long)from, (unsigned long)from_len };
	return lx_socketcall(SYS_RECVFROM, args);
}


inline ssize_t lx_sendto(int s, void *buf, Genode::size_t len, int flags,
                         struct sockaddr *to, socklen_t to_len)
{
	unsigned long args[6] = { s, (unsigned long)buf, len, flags,
	                          (unsigned long)to, (unsigned long)to_len };
	return lx_socketcall(SYS_SENDTO, args);
}

#else

inline int lx_socket(int domain, int type, int protocol)
{
	return lx_syscall(SYS_socket, domain, type, protocol);
}


inline int lx_connect(int sockfd, const struct sockaddr *serv_addr,
                      socklen_t addrlen)
{
	return lx_syscall(SYS_connect, sockfd, serv_addr, addrlen);
}


inline int lx_bind(int sockfd, const struct sockaddr *addr,
                   socklen_t addrlen)
{
	return lx_syscall(SYS_bind, sockfd, addr, addrlen);
}


inline int lx_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
	return lx_syscall(SYS_getsockname, s, name, namelen);
}


inline ssize_t lx_recvfrom(int s, void *buf, Genode::size_t len, int flags,
                           struct sockaddr *from, socklen_t *from_len)
{
	return lx_syscall(SYS_recvfrom, s, buf, len, flags, from, from_len);
}


inline ssize_t lx_sendto(int s, void *buf, Genode::size_t len, int flags,
                         struct sockaddr *to, socklen_t to_len)
{
	return lx_syscall(SYS_sendto, s, buf, len, flags, to, to_len);
}

#endif /* SYS_socketcall */


inline int lx_write(int fd, const void *buf, Genode::size_t count)
{
	return lx_syscall(SYS_write, fd, buf, count);
}


inline int lx_close(int fd)
{
	return lx_syscall(SYS_close, fd);
}


/*******************************************
 ** Functions used by the process library **
 *******************************************/

inline int lx_execve(const char *filename, char *const argv[],
                     char *const envp[])
{
	return lx_syscall(SYS_execve, filename, argv, envp);
}


inline void lx_exit(int status)
{
	lx_syscall(SYS_exit, status);
}


inline void lx_exit_group(int status)
{
	lx_syscall(SYS_exit_group, status);
}


/************************************************************
 ** Functions used by the env library and local rm session **
 ************************************************************/

/* O_CLOEXEC is a GNU extension so we provide it here */
enum { LX_O_CLOEXEC = 02000000 };

inline int lx_open(const char *pathname, int flags, mode_t mode = 0)
{
	return lx_syscall(SYS_open, pathname, flags, mode);
}


inline void *lx_mmap(void *start, Genode::size_t length, int prot, int flags,
                     int fd, off_t offset)
{
#ifdef _LP64
	return (void *)lx_syscall(SYS_mmap, start, length, prot, flags, fd, offset);
#else
	return (void *)lx_syscall(SYS_mmap2, start, length, prot, flags, fd, offset/4096);
#endif /* _LP64 */
}


inline int lx_munmap(void *addr, size_t length)
{
	return lx_syscall(SYS_munmap, addr, length);
}


/**
 * Exclude local virtual memory area from being used by mmap
 *
 * \param base  base address of area to reserve
 * \param size  number of bytes to reserve
 *
 * \return  start of allocated reserved area, or ~0 on failure
 */
inline Genode::addr_t lx_vm_reserve(Genode::addr_t base, Genode::size_t size)
{
	/* we cannot include sys/mman.h from here */
	enum {
		LX_MAP_PRIVATE   = 0x02,
		LX_MAP_FIXED     = 0x10,
		LX_MAP_ANONYMOUS = 0x20,
		LX_PROT_NONE     = 0x0
	};

	int const flags = LX_MAP_ANONYMOUS | LX_MAP_PRIVATE
	                | (base ? LX_MAP_FIXED : 0);

	void * const res = lx_mmap((void *)base, size, LX_PROT_NONE, flags, -1, 0);

	if (base)
		return ((Genode::addr_t)res == base) ? base : ~0;
	else
		return (Genode::addr_t)res;
}


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

inline int lx_stat(const char *path, struct stat64 *buf)
{
#ifdef _LP64
	return lx_syscall(SYS_stat, path, buf);
#else
	return lx_syscall(SYS_stat64, path, buf);
#endif
}

/***********************************************************************
 ** Functions used by thread lib and core's cancel-blocking mechanism **
 ***********************************************************************/

enum {
	LX_SIGUSR1   = 10,  /* used for cancel-blocking mechanism */
	LX_SIGCHLD   = 17,  /* child process changed state, i.e., terminated */
	LX_SIGCANCEL = 32,  /* accoring to glibc, this equals SIGRTMIN,
	                       used for killing threads */
};


struct kernel_sigaction
{
	void         (*handler)(int);
	unsigned long  flags;
	void         (*restorer)(void);
	sigset_t       mask;
};


inline int lx_sigemptyset(sigset_t *set)
{
	if (set == 0)
		return -1;
	Genode::memset(set, 0, sizeof(sigset_t));
	return 0;
}


#ifdef _LP64
extern "C" void lx_restore_rt (void);
#endif

/**
 * Simplified binding for sigaction system call
 */
inline int lx_sigaction(int signum, void (*handler)(int))
{
	struct kernel_sigaction act;
	act.handler = handler;

#ifdef _LP64
	/*
	 * The SA_RESTORER flag is not officially documented, but used internally
	 * by the glibc implementation of sigaction(). Without specifying this flag
	 * tgkill() does not work on x86_64. The restorer function gets called
	 * when leaving the signal handler and it should call the rt_sigreturn syscall.
	 */
	enum { SA_RESTORER = 0x04000000 };
	act.flags    = SA_RESTORER;
	act.restorer = lx_restore_rt;;
#else
	act.flags    = 0;
	act.restorer = 0;
#endif
	lx_sigemptyset(&act.mask);

	return lx_syscall(SYS_rt_sigaction, signum, &act, 0UL, _NSIG/8);
}


/**
 * Send signal to process
 *
 * This function is used by core to kill processes.
 */
inline int lx_kill(int pid, int signal)
{
	return lx_syscall(SYS_kill, pid, signal);
}


/**
 * Send signal to thread
 *
 * This function is used by core to cancel blocking operations of
 * threads, and by the thread library to kill threads.
 */
inline int lx_tgkill(int pid, int tid, int signal)
{
	return lx_syscall(SYS_tgkill, pid, tid, signal);
}


inline int lx_create_thread(void (*entry)(void *), void *stack, void *arg)
{
	int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
	          | CLONE_THREAD | CLONE_SYSVSEM;

	/*
	 * The syscall binding for clone does not exist in the FreeBSD libc, which
	 * we are using as libc for Genode. In glibc, clone is implemented as a
	 * assembler binding without external libc references. Hence, we are safe
	 * to rely on the glibc version of 'clone' here.
	 */
	return lx_clone((int (*)(void *))entry, stack, flags, arg);
}


inline int lx_create_process(int (*entry)(void *), void *stack, void *arg)
{
	int flags = CLONE_VFORK | SIGCHLD;
	return lx_clone((int (*)(void *))entry, stack, flags, arg);
}


inline pid_t lx_getpid() { return lx_syscall(SYS_getpid); }
inline pid_t lx_gettid() { return lx_syscall(SYS_gettid); }
inline uid_t lx_getuid() { return lx_syscall(SYS_getuid); }


/************************************
 ** Functions used by lock library **
 ************************************/

struct timespec;

inline int lx_nanosleep(const struct timespec *req, struct timespec *rem)
{
	return lx_syscall(SYS_nanosleep, req, rem);
}


/**
 * Signal set corrsponding to glibc's 'sigset_t'
 */
class Lx_sigset
{
	unsigned long int _value[_SIGSET_NWORDS];

	public:

		/**
		 * Constructor
		 */
		Lx_sigset() { }

		/**
		 * Constructor
		 *
		 * \param signum  set specified entry of sigset
		 */
		Lx_sigset(int signum)
		{

			for (unsigned i = 0; i < _SIGSET_NWORDS; i++)
				_value[i] = 0;

			/*
			 * Both '__sigword' and '__sigmask' are macros, defined in the
			 * glibc header file 'bits/sigset.h' and not external functions.
			 * Therefore we can use them here without getting into conflicts
			 * with the linkage of another libc.
			 */
			_value[__sigword(signum)] = __sigmask(signum);
		}

		bool is_set(int signum) {
			return _value[__sigword(signum)] && __sigmask(signum); }
};


/**
 * Check if signal is pending
 *
 * \return true if signal is pending
 */
inline bool lx_sigpending(int signum)
{
	Lx_sigset sigset;
	lx_syscall(SYS_rt_sigpending, &sigset, _NSIG/8);
	return sigset.is_set(signum);
}


/**
 * Set signal mask state
 *
 * \param signum  signal to mask or unmask
 * \param state   mask state for the signal,
 *                true enables the signal,
 *                false blocks the signal
 */
inline bool lx_sigsetmask(int signum, bool state)
{
	Lx_sigset old_sigmask, sigset(signum);
	lx_syscall(SYS_rt_sigprocmask, state ? SIG_UNBLOCK : SIG_BLOCK, &sigset, &old_sigmask, _NSIG/8);
	return old_sigmask.is_set(signum);
}

#endif /* _PLATFORM__LINUX_SYSCALLS_H_ */
