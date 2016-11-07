/*
 * \brief  Thread-environment support common to all programs
 * \author Martin Stein
 * \date   2013-12-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/stdint.h>
#include <base/env.h>
#include <base/log.h>

#include <linux_syscalls.h>

using namespace Genode;

extern addr_t * __initial_sp;

/*
 * Define 'lx_environ' pointer.
 */
char **lx_environ;

/**
 * Natively aligned memory location used in the lock implementation
 */
int main_thread_futex_counter __attribute__((aligned(sizeof(addr_t))));

/**
 * Genode console hook
 */
extern "C" int stdout_write(char const *);

/*
 * Core lacks the hook, so provide a base-linux specific weak implementation
 */
extern "C" __attribute__((weak)) int stdout_write(char const *s)
{
	raw(s);
	return Genode::strlen(s);
}

/**
 * Signal handler for exceptions like segmentation faults
 */
void exception_signal_handler(int signum)
{
	char const *reason = nullptr;

	switch (signum) {
	case LX_SIGILL:  reason = "Illegal instruction";      break;
	case LX_SIGBUS:  reason = "Bad memory access";        break;
	case LX_SIGFPE:  reason = "Floating point exception"; break;
	case LX_SIGSEGV: reason = "Segmentation fault";       break;

	default: /* unexpected signal */ return;
	}

	/*
	 * We can't use Genode::printf() as the exception may have occurred in the
	 * Genode console library itself, which uses a mutex. Therefore, we use
	 * Genode::snprintf() and call the console hook directly to minimize
	 * overlaps with other code paths.
	 */
	static char msg[128];
	snprintf(msg, sizeof(msg),
	         ESC_ERR "%s (signum=%d), see Linux kernel log for details" ESC_END "\n",
	         reason, signum);
	stdout_write(msg);

	/*
	 * We reset the signal handler to SIG_DFL and trigger exception again,
	 * i.e., terminate the process.
	 */
	lx_sigaction(signum, nullptr, false);
	return;
}


void lx_exception_signal_handlers()
{
	/* use alternate stack in fatal-signal handlers */
	lx_sigaction(LX_SIGILL,  exception_signal_handler, true);
	lx_sigaction(LX_SIGBUS,  exception_signal_handler, true);
	lx_sigaction(LX_SIGFPE,  exception_signal_handler, true);
	lx_sigaction(LX_SIGSEGV, exception_signal_handler, true);
}


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	/*
	 * Initialize the 'lx_environ' pointer
	 *
	 * environ = &argv[argc + 1]
	 * __initial_sp[0] = argc (always 1 in Genode)
	 * __initial_sp[1] = argv[0]
	 * __initial_sp[2] = NULL
	 * __initial_sp[3] = environ
	 */
	lx_environ = (char**)&__initial_sp[3];

	lx_exception_signal_handlers();
}
