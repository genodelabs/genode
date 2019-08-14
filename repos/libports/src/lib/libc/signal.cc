/*
 * \brief  POSIX signals
 * \author Emery Hemingway
 * \date   2015-10-30
 *
 * Signal related procedures to be overidden by Noux
 */

/*
 * Copyright (C) 2006-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
extern "C" {
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
}

/* Genode includes */
#include <base/log.h>


extern "C" __attribute__((weak))
int sigprocmask(int how, const sigset_t *set, sigset_t *old_set)
{
	/* no signals should be expected, so report all signals blocked */
	if (old_set != NULL)
		sigfillset(old_set);

	if (set == NULL)
		return 0;

	switch (how) {
	case SIG_BLOCK:   return 0;
	case SIG_SETMASK: return 0;
	case SIG_UNBLOCK: return 0;
	}
	errno = EINVAL;
	return -1;
}


extern "C"
int __sys_sigprocmask(int how, const sigset_t *set, sigset_t *old) {
	return sigprocmask(how, set, old); }


extern "C"
int __libc_sigprocmask(int how, const sigset_t *set, sigset_t *old) {
	return sigprocmask(how, set, old); }


/* wrapper from text-relocated i386-assembler call to PLT call */
extern "C" int __i386_libc_sigprocmask(int how, const sigset_t *set, sigset_t *old) __attribute__((visibility("hidden")));
extern "C" int __i386_libc_sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
	return __libc_sigprocmask(how, set, old);
}


extern "C" pid_t wait(int *istat) {
	return wait4(WAIT_ANY, istat, 0, NULL); }


extern "C" pid_t waitpid(pid_t pid, int *istat, int options) {
	return wait4(pid, istat, options, NULL); }


extern "C" pid_t _waitpid(pid_t pid, int *istat, int options) {
	return wait4(pid, istat, options, NULL); }


extern "C" __attribute__((weak))
pid_t wait6(idtype_t, id_t, int*, int, struct __wrusage*, siginfo_t*)
{
	Genode::warning(__func__, " not implemented");
	errno = ENOSYS;
	return -1;
}

extern "C"
pid_t __sys_wait6(idtype_t idtype, id_t id, int *status, int options,
                 struct __wrusage *wrusage, siginfo_t *infop) {
	return wait6(idtype, id, status, options, wrusage, infop); }


extern "C" int waitid(idtype_t idtype, id_t id, siginfo_t *info, int flags)
{
	int status;
	pid_t ret = wait6(idtype, id, &status, flags, NULL, info);

	/*
	 * According to SUSv4, waitid() shall not return a PID when a
	 * process is found, but only 0.  If a process was actually
	 * found, siginfo_t fields si_signo and si_pid will be
	 * non-zero.  In case WNOHANG was set in the flags and no
	 * process was found those fields are set to zero using
	 * memset() below.
	 */
	if (ret == 0 && info != NULL)
		*info = siginfo_t { 0 };
	else if (ret > 0)
		ret = 0;
	return (ret);
}
