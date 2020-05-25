/*
 * \brief  Linux system-call bindings
 * \author Norman Feske
 * \author Stefan Thöni
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
 * Copyright (C) 2019 gapfruit AG
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
#include <base/snprintf.h>
#include <base/log.h>
#include <base/sleep.h>


/*
 * Resolve ambiguity between 'Genode::size_t' and the host's header's 'size_t'.
 */
#define size_t __SIZE_TYPE__

/* Linux includes */
#include <sys/cdefs.h>   /* include first to avoid double definition of '__always_inline' */
#include <linux/futex.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>


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


inline pid_t lx_getpid() { return lx_syscall(SYS_getpid); }
inline pid_t lx_gettid() { return lx_syscall(SYS_gettid); }
inline uid_t lx_getuid() { return lx_syscall(SYS_getuid); }


inline int lx_write(int fd, const void *buf, Genode::size_t count)
{
	return lx_syscall(SYS_write, fd, buf, count);
}


inline int lx_close(int fd)
{
	return lx_syscall(SYS_close, fd);
}


inline int lx_dup(int fd)
{
	return lx_syscall(SYS_dup, fd);
}


inline int lx_dup2(int fd, int to)
{
	return lx_syscall(SYS_dup2, fd, to);
}


/*****************************************
 ** Functions used by the IPC framework **
 *****************************************/

#include <linux/net.h>

struct Lx_sd
{
	int value;

	bool valid() const { return value >= 0; }

	static Lx_sd invalid() { return Lx_sd{-1}; }

	uint64_t inode() const
	{
#ifdef __NR_fstat64
		struct stat64 statbuf { };
		(void)lx_syscall(SYS_fstat64, value, &statbuf);
#else
		struct stat statbuf { };
		(void)lx_syscall(SYS_fstat, value, &statbuf);
#endif /* __NR_fstat64 */
		return statbuf.st_ino;
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, "socket=", value);

		if (value >= 0)
			Genode::print(out, ",inode=", inode());
	}
};


struct Lx_epoll_sd { int value; };


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


inline int lx_sendmsg(Lx_sd sockfd, const struct msghdr *msg, int flags)
{
	long args[3] = { sockfd.value, (long)msg, flags };
	return lx_socketcall(SYS_SENDMSG, args);
}


inline int lx_recvmsg(Lx_sd sockfd, struct msghdr *msg, int flags)
{
	long args[3] = { sockfd.value, (long)msg, flags };
	return lx_socketcall(SYS_RECVMSG, args);
}

#else

inline int lx_socketpair(int domain, int type, int protocol, int sd[2])
{
	return lx_syscall(SYS_socketpair, domain, type, protocol, (unsigned long)sd);
}


inline int lx_sendmsg(Lx_sd sockfd, const struct msghdr *msg, int flags)
{
	return lx_syscall(SYS_sendmsg, sockfd.value, msg, flags);
}


inline int lx_recvmsg(Lx_sd sockfd, struct msghdr *msg, int flags)
{
	return lx_syscall(SYS_recvmsg, sockfd.value, msg, flags);
}

/* TODO add missing socket system calls */

#endif /* SYS_socketcall */


struct Lx_socketpair
{
	Lx_sd local  { -1 };
	Lx_sd remote { -1 };

	Lx_socketpair()
	{
		int sd[2];
		sd[0] = -1; sd[1] = -1;

		int const ret = lx_socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, sd);
		if (ret < 0) {
			Genode::raw(lx_getpid(), ":", lx_gettid(), " lx_socketpair failed with ", ret);
			Genode::sleep_forever();
		}

		local  = Lx_sd { sd[0] };
		remote = Lx_sd { sd[1] };
	}
};


inline Lx_epoll_sd lx_epoll_create()
{
	int const ret = lx_syscall(SYS_epoll_create, 1);
	if (ret < 0) {
		/*
		 * No recovery possible, just leave a diagnostic message and block
		 * forever.
		 */
		Genode::raw(lx_getpid(), ":", lx_gettid(), " lx_epoll_create failed with ", ret);
		Genode::sleep_forever();
	}
	return Lx_epoll_sd { ret };
}


inline int lx_epoll_ctl(Lx_epoll_sd epoll, int op, Lx_sd fd, epoll_event *event)
{
	return lx_syscall(SYS_epoll_ctl, epoll.value, op, fd.value, event);
}


inline int lx_epoll_wait(Lx_epoll_sd epoll, struct epoll_event *events,
                         int maxevents, int timeout)
{
	return lx_syscall(SYS_epoll_wait, epoll.value, events, maxevents, timeout);
}


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
	LX_NSIG      = 64,  /* number of different signals supported */
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

	return lx_syscall(SYS_rt_sigaction, signum, &act, 0UL, LX_NSIG/8);
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
	private:

		enum {
			BITS_PER_LONG = 8 * sizeof(unsigned long),
			SIGSET_SIZE   = LX_NSIG / BITS_PER_LONG
		};

		unsigned long _value[SIGSET_SIZE];

		/*
		 * Both '__sigword' and '__sigmask' were macros defined in the glibc
		 * header file 'bits/sigset.h'. The macros were moved in later versions
		 * and, therefore, we implement them explicitly.
		 */

		/* return mask that includes the bit for 'signum' only (was __sigmask) */
		unsigned long _mask(int signum)
		{
			return (1UL) << ((signum - 1) % BITS_PER_LONG);
		}

		/* return word index for 'signum' (was __sigword)  */
		unsigned _word(int signum)
		{
			return (signum - 1) / BITS_PER_LONG;
		}

	public:

		/**
		 * Constructor
		 */
		Lx_sigset() { Genode::memset(_value, 0, sizeof(_value)); }

		/**
		 * Constructor
		 *
		 * \param signum  set specified entry of sigset
		 */
		Lx_sigset(int signum) : Lx_sigset()
		{
			_value[_word(signum)] = _mask(signum);
		}

		bool is_set(int signum)
		{
			return _value[_word(signum)] && _mask(signum);
		}
};


/**
 * Check if signal is pending
 *
 * \return true if signal is pending
 */
inline bool lx_sigpending(int signum)
{
	Lx_sigset sigset;
	lx_syscall(SYS_rt_sigpending, &sigset, sizeof(sigset));
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
	lx_syscall(SYS_rt_sigprocmask, state ? SIG_UNBLOCK : SIG_BLOCK, &sigset, &old_sigmask, sizeof(sigset));
	return old_sigmask.is_set(signum);
}


inline int lx_prctl(int option, unsigned long arg2, unsigned long arg3,
                                unsigned long arg4, unsigned long arg5)
{
	return lx_syscall(SYS_prctl, option, arg2, arg3, arg4, arg5);
}


inline int lx_seccomp(int option, int flag, void* program)
{
	return lx_syscall(SYS_seccomp, option, flag, program);
}

#endif /* _LIB__SYSCALL__LINUX_SYSCALLS_H_ */
