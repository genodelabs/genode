/*
 * \brief  Dummy implementations
 * \author Norman Feske
 * \date   2008-10-10
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <stddef.h>
#include <errno.h>

extern "C" {

	typedef long DUMMY;

#define DUMMY(retval, name) __attribute__((weak))		\
DUMMY name(void) {						\
	PDBG( #name " called, not implemented");		\
	errno = ENOSYS;						\
	return retval;						\
}

DUMMY(-1, access)
DUMMY(-1, chmod)
DUMMY(-1, chown)
DUMMY(-1, chroot)
DUMMY( 0, crypt)
DUMMY( 0, dbopen)
DUMMY(-1, dup)
DUMMY( 0, __default_hash)
DUMMY(-1, _dup2)
DUMMY(-1, dup2)
DUMMY( 0, endpwent)
DUMMY(-1, _execve)
DUMMY(-1, execve)
DUMMY(-1, fchmod)
DUMMY(-1, fchown)
DUMMY(-1, feholdexcept)
DUMMY(-1, fegetenv)
DUMMY(-1, feraiseexcept)
DUMMY(-1, feupdateenv)
DUMMY(-1, flock)
DUMMY(-1, fork)
DUMMY(-1, _fpathconf)
DUMMY(-1, fpathconf)
DUMMY(-1, freebsd7___semctl)
DUMMY(-1, fstatat)
DUMMY(-1, getcontext)
DUMMY( 0, __getcwd)
DUMMY( 0, getdtablesize)
DUMMY( 0, getegid)
DUMMY( 0, geteuid)
DUMMY(-1, getfsstat)
DUMMY( 0, getgid)
DUMMY(-1, getgroups)
DUMMY( 0, gethostbyname)
DUMMY( 0, _getlogin)
DUMMY(-1, getnameinfo)
DUMMY(-1, getpid)
DUMMY( 0, getservbyname)
DUMMY(-1, getsid)
DUMMY(-1, getppid)
DUMMY(-1, getpgrp)
DUMMY(-1, getpriority)
DUMMY( 0, getpwent)
DUMMY( 0, getpwnam)
DUMMY( 0, getpwuid)
DUMMY( 0, getpwuid_r)
DUMMY(-1, __getpty)
DUMMY(-1, _getpty)
DUMMY(-1, getrusage)
DUMMY( 0, getuid)
DUMMY(-1, __has_sse)
DUMMY(-1, host_detect_local_cpu)
DUMMY(-1, kill)
DUMMY(-1, ksem_close)
DUMMY(-1, ksem_destroy)
DUMMY(-1, ksem_getvalue)
DUMMY(-1, ksem_open)
DUMMY(-1, ksem_post)
DUMMY(-1, ksem_timedwait)
DUMMY(-1, ksem_trywait)
DUMMY(-1, ksem_unlink)
DUMMY(-1, ksem_wait)
DUMMY(-1, link)
DUMMY(-1, lstat)
DUMMY(-1, madvise)
DUMMY(-1, mkfifo)
DUMMY(-1, mknod)
DUMMY(-1, mprotect)
DUMMY( 0, ___mtctxres)
DUMMY(-1, nanosleep)
DUMMY(-1, __nsdefaultsrc)
DUMMY(-1, _nsdispatch)
DUMMY(-1, _openat)
DUMMY(-1, pathconf)
DUMMY(-1, pthread_create)
DUMMY(-1, regcomp)
DUMMY(-1, regexec)
DUMMY(-1, regfree)
DUMMY(-1, rmdir)
DUMMY(-1, sbrk)
DUMMY(-1, sched_setparam)
DUMMY(-1, sched_setscheduler)
DUMMY(-1, sched_yield)
DUMMY(-1, _select)
DUMMY(-1, __semctl)
DUMMY(-1, setcontext)
DUMMY(-1, setegid)
DUMMY(-1, seteuid)
DUMMY(-1, setgid)
DUMMY(-1, setuid)
DUMMY(-1, setgroups)
DUMMY(-1, setitimer)
DUMMY(-1, setpassent)
DUMMY(-1, setpgid)
DUMMY(-1, setpriority)
DUMMY( 0, setpwent)
DUMMY(-1, setregid)
DUMMY(-1, setreuid)
DUMMY(-1, setrlimit)
DUMMY(-1, setsid)
DUMMY(-1, _sigaction)
DUMMY(-1, sigaction)
DUMMY(-1, sigblock)
DUMMY(-1, sigpause)
DUMMY(-1, _sigprocmask)
DUMMY(-1, sigprocmask)
DUMMY(-1, _sigsuspend)
DUMMY(-1, sigsuspend)
DUMMY(-1, socketpair)
DUMMY(-1, stat)
DUMMY(-1, statfs)
DUMMY(-1, symlink)
DUMMY( 0, sync)
DUMMY(-1, __test_sse)
DUMMY(-1, truncate)
DUMMY( 0, umask)
DUMMY(-1, _umtx_op)
DUMMY(-1, utimes)
DUMMY(-1, utrace)
DUMMY(-1, vfork)
DUMMY(-1, _wait4)

void ksem_init(void)
{
	PDBG("ksem_init called, not yet implemented!");
	while (1);
}

} /* extern "C" */

