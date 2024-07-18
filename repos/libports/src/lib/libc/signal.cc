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

/* libc-internal includes */
#include <internal/types.h>
#include <internal/init.h>
#include <internal/signal.h>
#include <internal/errno.h>

using namespace Libc;


static Libc::Signal *_signal_ptr;


void Libc::init_signal(Signal &signal)
{
	_signal_ptr = &signal;
}


void Libc::Signal::_execute_signal_handler(unsigned n)
{
	if (signal_action[n].sa_flags & SA_SIGINFO) {
		signal_action[n].sa_sigaction(n, 0, 0);
		return;
	}

	if (signal_action[n].sa_handler == SIG_DFL) {
		switch (n) {
			case SIGCHLD:
			case SIGWINCH:
				/* ignore */
				break;
			default:
				/*
				 * Trigger the termination of the process.
				 *
				 * We cannot call 'exit' immediately as the exiting code (e.g.,
				 * 'atexit' handlers) may potentially issue I/O operations such
				 * as FD sync and close. Hence, we just flag the intention to
				 * exit and issue the actual exit call at the end of
				 * 'Signal::execute_signal_handlers'
				 */
				_exit      = true;
				_exit_code = (n << 8) | EXIT_FAILURE;
		}
	} else if (signal_action[n].sa_handler == SIG_IGN) {
		/* do nothing */
	} else {
		signal_action[n].sa_handler(n);
	}
}


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


extern "C" int sigaction(int, const struct sigaction *, struct sigaction *) __attribute__((weak));


extern "C" int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	if ((signum < 1) || (signum > NSIG)) {
		errno = EINVAL;
		return -1;
	}

	if (oldact)
		*oldact = _signal_ptr->signal_action[signum];

	if (act)
		_signal_ptr->signal_action[signum] = *act;

	return 0;
}


extern "C" __sighandler_t * signal(int sig, __sighandler_t * func)
{
	struct sigaction oact { }, act { };
	act.sa_handler = func;

	if (sigaction(sig, &act, &oact) == 0)
		return oact.sa_handler;

	errno = EINVAL;
	return SIG_ERR;
}


extern "C" int       _sigaction(int, const struct sigaction *, struct sigaction *) __attribute__((weak, alias("sigaction")));
extern "C" int  __sys_sigaction(int, const struct sigaction *, struct sigaction *) __attribute__((weak, alias("sigaction")));
extern "C" int __libc_sigaction(int, const struct sigaction *, struct sigaction *) __attribute__((weak, alias("sigaction")));


extern "C" int kill(pid_t pid, int signum)
{
	if (_signal_ptr->local_pid(pid)) {
		_signal_ptr->charge(signum);
		_signal_ptr->execute_signal_handlers();
		return 0;
	} else {
		warning("submitting signals to remote processes via 'kill' not supported");
		return Libc::Errno(EPERM);
	}
}


extern "C" int       _kill(pid_t, int) __attribute__((weak, alias("kill")));
extern "C" int  __sys_kill(pid_t, int) __attribute__((weak, alias("kill")));
extern "C" int __libc_kill(pid_t, int) __attribute__((weak, alias("kill")));


extern "C" pid_t wait(int *istat) {
	return wait4(WAIT_ANY, istat, 0, NULL); }


extern "C" pid_t waitpid(pid_t pid, int *istat, int options) {
	return wait4(pid, istat, options, NULL); }


extern "C" pid_t _waitpid(pid_t pid, int *istat, int options) {
	return wait4(pid, istat, options, NULL); }


extern "C" __attribute__((weak))
pid_t wait6(idtype_t, id_t, int*, int, struct __wrusage*, siginfo_t*)
{
	warning(__func__, " not implemented");
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


extern "C" int raise(int sig)
{
	char const *signame = sys_signame[sig];

	switch(sig) {
	case SIGQUIT:
	case SIGABRT:
	case SIGKILL:
		error(__func__, "(", signame, ")");
		exit(-1);
	default:
		warning(__func__, "(", signame, ") not implemented");
		return Libc::Errno(EINVAL);
	};
}


extern "C" int sigaltstack(stack_t const * const ss, stack_t * const old_ss)
{
	if (!_signal_ptr)
		return Errno(EINVAL);

	auto &signal = *_signal_ptr;

	if (ss) {
		auto myself = Thread::myself();
		if (!myself)
			return Errno(EINVAL);

		if (ss->ss_flags & SS_DISABLE) {
			/* on disable use the default signal stack */
			signal.use_alternative_stack(nullptr);

			warning("leaking secondary stack memory");

		} else {
			if (ss->ss_sp)
				warning(__func__, " using self chosen stack is not"
				                  " supported - stack ptr is ignored !!!");

			void * stack = myself->alloc_secondary_stack("sigaltstack",
			                                             ss->ss_size);

			signal.use_alternative_stack(stack);
		}
	}

	if (old_ss && ss && !(ss->ss_flags & SS_DISABLE))
		old_ss->ss_flags = SS_DISABLE;

	return 0;
}
