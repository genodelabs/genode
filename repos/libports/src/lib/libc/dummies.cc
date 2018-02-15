/*
 * \brief  Dummy implementations
 * \author Norman Feske
 * \date   2008-10-10
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
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

#include <db.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <pwd.h>

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


DUMMY(int   , -1, chmod, (const char *, mode_t))
DUMMY(int   , -1, chown, (const char *, uid_t, gid_t))
DUMMY(int   , -1, chroot, (const char *))
DUMMY(char *,  0, crypt, (const char *, const char *))
DUMMY(DB *  ,  0, dbopen, (const char *, int, int, DBTYPE, const void *))
DUMMY(u_int32_t, 0, __default_hash, (const void *, size_t));
DUMMY(void  ,   , endpwent, (void))
DUMMY(int   ,  0, fchmod, (int, mode_t))
DUMMY(int   , -1, fchown, (int, uid_t, gid_t))
DUMMY(int   , -1, flock, (int, int))
DUMMY(pid_t , -1, fork, (void))
DUMMY_SILENT(long  , -1, _fpathconf, (int, int))
DUMMY(long  , -1, fpathconf, (int, int))
DUMMY(int   , -1, freebsd7___semctl, (void))
DUMMY(int   , -1, fstatat, (int, const char *, struct stat *, int))
DUMMY(int   , -1, getcontext, (ucontext_t *))
DUMMY(gid_t ,  0, getegid, (void))
DUMMY(uid_t ,  0, geteuid, (void))
DUMMY(int   , -1, getfsstat, (struct statfs *, long, int))
DUMMY(gid_t ,  0, getgid, (void))
DUMMY(int   , -1, getgroups, (int, gid_t *))
DUMMY(struct hostent *, 0, gethostbyname, (const char *))
DUMMY(char *,  0, _getlogin, (void))
DUMMY(int   , -1, getnameinfo, (const sockaddr *, socklen_t, char *, size_t, char *, size_t, int))
DUMMY_SILENT(pid_t , -1, getpid, (void))
DUMMY(struct servent *, 0, getservbyname, (const char *, const char *))
DUMMY(int   , -1, getsid, (pid_t))
DUMMY(pid_t , -1, getppid, (void))
DUMMY(pid_t , -1, getpgrp, (void))
DUMMY(int   , -1, getpriority, (int, int))
DUMMY(struct passwd *, 0, getpwent, (void))
DUMMY(struct passwd *, 0, getpwnam, (const char *))
DUMMY(struct passwd *, 0, getpwuid, (uid_t))
DUMMY(int   ,  0, getpwuid_r, (uid_t, struct passwd *, char *, size_t, struct passwd **))
DUMMY(int   , -1, getrusage, (int, rusage *))
DUMMY(uid_t ,  0, getuid, (void))
DUMMY(int   , -1, kill, (pid_t, int))
DUMMY(int   , -1, link, (const char *, const char *))
DUMMY(int   , -1, mkfifo, (const char *, mode_t))
DUMMY(int   , -1, mknod, (const char *, mode_t, dev_t))
DUMMY(int   , -1, mprotect, (const void *, size_t, int))
DUMMY(void *,  0, ___mtctxres, (void))
DUMMY(void *,  0, __nsdefaultsrc, (void))
DUMMY(int   , -1, _nsdispatch, (void))
DUMMY(long  , -1, pathconf, (const char *, int))
DUMMY(int   , -1, pthread_create, (pthread_t *, const pthread_attr_t *, void *(*)(void *), void *))
DUMMY(int   , -1, rmdir, (const char *))
DUMMY(void *,  0, sbrk, (intptr_t))
DUMMY(int   , -1, sched_setparam, (pid_t, const sched_param *))
DUMMY(int   , -1, sched_setscheduler, (pid_t, int, const sched_param *))
DUMMY(int   , -1, sched_yield, (void))
DUMMY(int   , -1, __semctl, (void))
DUMMY(int   , -1, setcontext, (const ucontext_t *))
DUMMY(int   , -1, setegid, (uid_t))
DUMMY(int   , -1, seteuid, (uid_t))
DUMMY(int   , -1, setgid, (gid_t))
DUMMY(int   , -1, setuid, (uid_t))
DUMMY(int   , -1, setgroups, (int, const gid_t *))
DUMMY(int   , -1, setitimer, (int, const itimerval *, itimerval *))
DUMMY(int   , -1, setpassent, (int))
DUMMY(int   , -1, setpgid, (pid_t, pid_t))
DUMMY(int   , -1, setpriority, (int, int, int))
DUMMY(void  ,   , setpwent, (void))
DUMMY(int   , -1, setregid, (gid_t, gid_t))
DUMMY(int   , -1, setreuid, (uid_t, uid_t))
DUMMY(int   , -1, setrlimit, (int, const rlimit *))
DUMMY(pid_t , -1, setsid, (void))
DUMMY_SILENT(int   , -1, _sigaction, (int, const struct sigaction *, struct sigaction *))
DUMMY(int   , -1, sigaction, (int, const struct sigaction *, struct sigaction *))
DUMMY(int   , -1, sigblock, (int))
DUMMY(int   , -1, sigpause, (int))
DUMMY(int   , -1, _sigsuspend, (const sigset_t *))
DUMMY(int   , -1, sigsuspend, (const sigset_t *))
DUMMY(int   , -1, socketpair, (int, int, int, int *))
DUMMY(int   , -1, stat, (const char *, struct stat *))
DUMMY(int   , -1, statfs, (const char *, struct statfs *))
DUMMY(void  ,   , sync, (void))
DUMMY(int   , -1, truncate, (const char *, off_t))
DUMMY_SILENT(mode_t,  0, umask, (mode_t))
DUMMY(int   ,  0, utimes, (const char *, const timeval *))
DUMMY(pid_t , -1, vfork, (void))
DUMMY(pid_t , -1, _wait4, (pid_t, int *, int, struct rusage *))
DUMMY(int, -1, semget, (key_t, int, int))
DUMMY(int, -1, semop, (key_t, int, int))


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

} /* extern "C" */

