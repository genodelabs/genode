/*
 * \brief  Linux system-call bindings
 * \author Norman Feske
 * \date   2008-10-22
 *
 * This file is meant to be internally used by the framework. It is not public
 * interface.
 *
 * From within the framework libraries, we have to use the Linux syscall
 * interface directly rather than relying on convenient libc functions to be
 * able to link this part of the framework to a custom libc. Otherwise, we
 * would end up with very nasty cyclic dependencies when using framework
 * functions such as IPC from the libc back end.
 *
 * The Linux syscall interface looks different for 32bit and 64bit system, in
 * particular regarding the socket interface. On 32bit systems, all socket
 * operations are invoked via the 'socketcall' syscall. On 64bit systems, the
 * different socket functions have distinct syscalls.
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SYSCALL__LINUX_SYSCALLS_H_
#define _LIB__SYSCALL__LINUX_SYSCALLS_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1 /* needed to enable the definition of 'stat64' */
#endif

/* Genode includes */
#include <util/string.h>
#include <base/printf.h>
#include <base/snprintf.h>
#include <base/log.h>

/*
 * Resolve ambiguity between 'Genode::size_t' and the host's header's 'size_t'.
 */
#define size_t __SIZE_TYPE__

/* Linux includes */
#include <linux/futex.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/mman.h>

#undef size_t


/***********************************
 ** Low-level debugging utilities **
 ***********************************/

extern "C" void wait_for_continue(void);

#define PRAW(fmt, ...)                                             \
	do {                                                           \
		char str[128];                                             \
		Genode::snprintf(str, sizeof(str),                         \
		                 ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__); \
		Genode::raw(Genode::Cstring(str));                         \
	} while (0)


/*********************************************************
 ** System-call bindings implemented in syscall library **
 *********************************************************/

extern "C" long lx_syscall(int number, ...);
extern "C" int  lx_clone(int (*fn)(void *), void *child_stack,
                         int flags, void *arg);


/*****************************************
 ** General syscalls used by base-linux **
 *****************************************/

inline int lx_write(int fd, const void *buf, Genode::size_t count)
{
	return lx_syscall(SYS_write, fd, buf, count);
}


inline int lx_close(int fd)
{
	return lx_syscall(SYS_close, fd);
}


inline int lx_dup2(int fd, int to)
{
	return lx_syscall(SYS_dup2, fd, to);
}


/*****************************************
 ** Functions used by the IPC framework **
 *****************************************/

#include <linux/net.h>

#ifdef SYS_socketcall

inline int lx_socketcall(int call, long *args)
{
	int res = lx_syscall(SYS_socketcall, call, args);
	return res;
}

inline int lx_socketpair(int domain, int type, int protocol, int sd[2])
{
	long args[4] = { domain, type, protocol, (long)sd };
	return lx_socketcall(SYS_SOCKETPAIR, args);
}


inline int lx_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	long args[3] = { sockfd, (long)msg, flags };
	return lx_socketcall(SYS_SENDMSG, args);
}


inline int lx_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	long args[3] = { sockfd, (long)msg, flags };
	return lx_socketcall(SYS_RECVMSG, args);
}


inline int lx_getpeername(int sockfd, struct sockaddr *name, socklen_t *namelen)
{
	long args[3] = { sockfd, (long)name, (long)namelen };
	return lx_socketcall(SYS_GETPEERNAME, args);
}

#else

inline int lx_socketpair(int domain, int type, int protocol, int sd[2])
{
	return lx_syscall(SYS_socketpair, domain, type, protocol, (unsigned long)sd);
}


inline int lx_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	return lx_syscall(SYS_sendmsg, sockfd, msg, flags);
}


inline int lx_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	return lx_syscall(SYS_recvmsg, sockfd, msg, flags);
}


inline int lx_getpeername(int sockfd, struct sockaddr *name, socklen_t *namelen)
{
	return lx_syscall(SYS_getpeername, sockfd, name, namelen);
}

/* TODO add missing socket system calls */

#endif /* SYS_socketcall */


/*******************************************
 ** Functions used by the process library **
 *******************************************/

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

inline void *lx_mmap(void *start, Genode::size_t length, int prot, int flags,
                     int fd, off_t offset)
{
#ifdef _LP64
	return (void *)lx_syscall(SYS_mmap, start, length, prot, flags, fd, offset);
#else
	return (void *)lx_syscall(SYS_mmap2, start, length, prot, flags, fd, offset/4096);
#endif /* _LP64 */
}


inline int lx_munmap(void *addr, Genode::size_t length)
{
	return lx_syscall(SYS_munmap, addr, length);
}


/***********************************************************************
 ** Functions used by thread lib and core's cancel-blocking mechanism **
 ***********************************************************************/

enum {
	LX_SIGINT    =  2,  /* used by core to catch Control-C */
	LX_SIGILL    =  4,  /* exception: illegal instruction */
	LX_SIGBUS    =  7,  /* exception: bus error, i.e., bad memory access */
	LX_SIGFPE    =  8,  /* exception: floating point */
	LX_SIGUSR1   = 10,  /* used for cancel-blocking mechanism */
	LX_SIGSEGV   = 11,  /* exception: segmentation violation */
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
inline int lx_sigaction(int signum, void (*handler)(int), bool altstack)
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
	act.restorer = lx_restore_rt;
#else
	act.flags    = 0;
	act.restorer = 0;
#endif

	/* use alternate signal stack if requested */
	act.flags |= altstack ? SA_ONSTACK : 0;

	lx_sigemptyset(&act.mask);

	return lx_syscall(SYS_rt_sigaction, signum, &act, 0UL, _NSIG/8);
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


/**
 * Alternate signal stack (handles also SIGSEGV in a safe way)
 */
inline int lx_sigaltstack(void *signal_stack, Genode::size_t stack_size)
{
	stack_t stack { signal_stack, 0, stack_size };

	return lx_syscall(SYS_sigaltstack, &stack, nullptr);
}


inline int lx_create_thread(void (*entry)(), void *stack, void *arg)
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

enum {
	LX_FUTEX_WAIT = FUTEX_WAIT,
	LX_FUTEX_WAKE = FUTEX_WAKE,
};

inline int lx_futex(const int *uaddr, int op, int val)
{
	return lx_syscall(SYS_futex, uaddr, op, val, 0, 0, 0);
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


#endif /* _LIB__SYSCALL__LINUX_SYSCALLS_H_ */
