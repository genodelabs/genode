/*
 * \brief  Dummy implementations
 * \author Norman Feske
 * \date   2008-10-10
 */

/*
 * Copyright (C) 2008-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <stddef.h>
#include <errno.h>

extern "C" {

#include <sys/file.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <sys/param.h>
#include <sys/cpuset.h>

#include <db.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <pwd.h>
#include <netinet/in.h>
#include <resolv.h>
#include <spinlock.h>
#include <spawn.h>
#include <ucontext.h>


#define DUMMY(ret_type, ret_val, name, args) __attribute__((weak)) \
ret_type name args \
{ \
	Genode::warning(__func__, ": " #name " not implemented"); \
	errno = ENOSYS;						\
	return ret_val; \
}


#define DUMMY_SILENT(ret_type, ret_val, name, args) __attribute__((weak)) \
ret_type name args \
{ \
	errno = ENOSYS;						\
	return ret_val; \
}


#define __SYS_DUMMY(ret_type, ret_val, name, args)\
	extern "C" __attribute__((weak)) \
	ret_type __sys_##name args \
	{ \
		Genode::warning(__func__, ": " #name " not implemented"); \
		errno = ENOSYS;						\
		return ret_val; \
	} \
	extern "C" __attribute__((weak)) \
	ret_type __libc_##name args \
	{ \
		Genode::warning(__func__, ": " #name " not implemented"); \
		errno = ENOSYS;						\
		return ret_val; \
	} \
	ret_type      _##name args __attribute__((weak, alias("__sys_" #name))); \
	ret_type         name args __attribute__((weak, alias("__sys_" #name))); \


#define __SYS_DUMMY_SILENT(ret_type, ret_val, name, args)\
	extern "C" __attribute__((weak)) \
	ret_type __sys_##name args \
	{ \
		errno = ENOSYS;						\
		return ret_val; \
	} \
	extern "C" __attribute__((weak)) \
	ret_type __libc_##name args \
	{ \
		errno = ENOSYS;						\
		return ret_val; \
	} \
	ret_type      _##name args __attribute__((weak, alias("__sys_" #name))); \
	ret_type         name args __attribute__((weak, alias("__sys_" #name))); \


DUMMY(int   , -1, chroot, (const char *))
DUMMY(int   , -1, cpuset_getaffinity, (cpulevel_t, cpuwhich_t, id_t, size_t, cpuset_t *))
DUMMY(char *,  0, crypt, (const char *, const char *))
DUMMY(DB *  ,  0, dbopen, (const char *, int, int, DBTYPE, const void *))
DUMMY(u_int32_t, 0, __default_hash, (const void *, size_t));
DUMMY_SILENT(long  , -1, _fpathconf, (int, int))
DUMMY(long  , -1, fpathconf, (int, int))
DUMMY(int   , -1, freebsd7___semctl, (void))
DUMMY(int   , -1, getcontext, (ucontext_t *))
DUMMY_SILENT(gid_t ,  0, getegid, (void))
DUMMY_SILENT(uid_t ,  0, geteuid, (void))
DUMMY_SILENT(gid_t ,  0, getgid, (void))
DUMMY(int   , -1, getgroups, (int, gid_t *))
DUMMY(struct hostent *, 0, gethostbyname, (const char *))
DUMMY(char *,  0, _getlogin, (void))
DUMMY(int   , -1, getnameinfo, (const sockaddr *, socklen_t, char *, size_t, char *, size_t, int))
DUMMY(struct servent *, 0, getservbyname, (const char *, const char *))
DUMMY(int   , -1, getsid, (pid_t))
DUMMY_SILENT(pid_t , -1, getppid, (void))
DUMMY(pid_t , -1, getpgrp, (void))
DUMMY(int   , -1, getpriority, (int, int))
DUMMY(int   , -1, getrusage, (int, rusage *))
DUMMY_SILENT(uid_t ,  0, getuid, (void))
DUMMY_SILENT(int   ,  1, isatty, (int))
DUMMY(int   , -1, link, (const char *, const char *))
DUMMY(int   ,  0, minherit, (void *, size_t, int))
DUMMY(int   , -1, mknod, (const char *, mode_t, dev_t))
DUMMY(int   , -1, mprotect, (void *, size_t, int))
DUMMY(void *,  0, ___mtctxres, (void))
DUMMY(void *,  0, __nsdefaultsrc, (void))
DUMMY(int   , -1, _nsdispatch, (void))
DUMMY(long  , -1, pathconf, (const char *, int))
DUMMY(void  ,   , pthread_set_name_np, (pthread_t, const char *))
DUMMY(int   , -1, posix_spawn_file_actions_addchdir_np, (posix_spawn_file_actions_t *, const char *))
DUMMY(int   , -1, rmdir, (const char *))
DUMMY(void *,  0, sbrk, (intptr_t))
DUMMY(int   , -1, sched_setparam, (pid_t, const sched_param *))
DUMMY(int   , -1, sched_setscheduler, (pid_t, int, const sched_param *))
DUMMY(int   , -1, sched_yield, (void))
DUMMY(int   , -1, __semctl, (void))
DUMMY_SILENT(int   , -1, sigaltstack, (const stack_t *, stack_t *))
DUMMY(int   , -1, setegid, (uid_t))
DUMMY(int   , -1, seteuid, (uid_t))
DUMMY(int   , -1, setgid, (gid_t))
DUMMY(int   , -1, setuid, (uid_t))
DUMMY(int   , -1, setgroups, (int, const gid_t *))
DUMMY(int   , -1, setitimer, (int, const itimerval *, itimerval *))
DUMMY(int   , -1, setpgid, (pid_t, pid_t))
DUMMY(int   , -1, setpriority, (int, int, int))
DUMMY(int   , -1, setregid, (gid_t, gid_t))
DUMMY(int   , -1, setreuid, (uid_t, uid_t))
DUMMY(int   , -1, setrlimit, (int, const rlimit *))
DUMMY(pid_t , -1, setsid, (void))
DUMMY(int   , -1, socketpair, (int, int, int, int *))
DUMMY_SILENT(mode_t,  0, umask, (mode_t))
DUMMY(int   ,  0, utimes, (const char *, const timeval *))
DUMMY(int, -1, semget, (key_t, int, int))
DUMMY(int, -1, semop, (key_t, int, int))
DUMMY(int   , -1, _umtx_op, (void *, int , u_long, void *, void *))
__SYS_DUMMY(int,    -1, aio_suspend, (const struct aiocb * const[], int, const struct timespec *));
__SYS_DUMMY(int   , -1, getfsstat, (struct statfs *, long, int))
__SYS_DUMMY(int, -1, kevent, (int, const struct kevent*, int, struct kevent *, int, const struct timespec*));
__SYS_DUMMY(void  ,   , map_stacks_exec, (void));
__SYS_DUMMY(int   , -1, ptrace, (int, pid_t, caddr_t, int));
__SYS_DUMMY(ssize_t, -1, sendmsg, (int s, const struct msghdr*, int));
__SYS_DUMMY(int   , -1, setcontext, (const ucontext_t *ucp));
__SYS_DUMMY(void	,   , spinlock_stub,   (spinlock_t *));
__SYS_DUMMY(void	,   , spinunlock_stub, (spinlock_t *));
__SYS_DUMMY(int, -1, swapcontext, (ucontext_t *, const ucontext_t *));
__SYS_DUMMY(int, -1, system, (const char *string));


/*****************
 ** File-system **
 *****************/

DUMMY(int,  0, fchmod, (int, mode_t))
DUMMY(int, -1, lockf, (int, int, off_t))
DUMMY_SILENT(int,  0, posix_fadvise, (int, off_t, off_t, int))
DUMMY(int, -1, chmod, (const char *, mode_t))
DUMMY(int, -1, chown, (const char *, uid_t, gid_t))
DUMMY(int, -1, fchown, (int, uid_t, gid_t))
DUMMY(int, -1, flock, (int, int))
DUMMY(int, -1, mkfifo, (const char *, mode_t))
DUMMY(void,  , sync, (void))
__SYS_DUMMY(int, -11, utimensat, (int, const char *, const struct timespec[2], int));
__SYS_DUMMY(int, -1, futimens, (int, const struct timespec[2]));
__SYS_DUMMY(int, -1, statfs, (const char *, struct statfs *))
__SYS_DUMMY(int, -1, truncate, (const char *, off_t))


/***********
 * Signals *
 ***********/

#include <signal.h>
#include <sys/thr.h>
DUMMY(int, -1, sigblock, (int))
DUMMY(int, -1, thr_kill2, (pid_t pid, long id, int sig));

__SYS_DUMMY(int, -1, sigsuspend, (const sigset_t *))
__SYS_DUMMY(int, -1, sigtimedwait, (const sigset_t *, siginfo_t *, const struct timespec *));
__SYS_DUMMY(int, -1, sigwaitinfo, (const sigset_t *, siginfo_t *));
__SYS_DUMMY(int, -1, sigwait, (const sigset_t *, int *));
__SYS_DUMMY(int, -1, thr_kill,  (long id, int sig));


void ksem_init(void)
{
	Genode::warning(__func__, " called, not yet implemented!");
	while (1);
}


int __attribute__((weak)) madvise(void *addr, size_t length, int advice)
{
	if (advice == MADV_DONTNEED)
		/* ignore hint */
		return 0;

	Genode::warning(__func__, " called, not implemented - ", addr, "+",
	                Genode::Hex(length), " advice=", advice);
	errno = ENOSYS;
	return -1;
}

const struct res_sym __p_type_syms[] = { };

} /* extern "C" */

