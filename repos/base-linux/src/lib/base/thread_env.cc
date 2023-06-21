/*
 * \brief  Thread-environment support common to all programs
 * \author Martin Stein
 * \date   2013-12-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/stdint.h>
#include <base/env.h>
#include <base/log.h>

#include <linux_syscalls.h>

/* base-internal includes */
#include <base/internal/globals.h>

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

	raw(reason, " (signum=", signum, "), see Linux kernel log for details");

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

void Genode::prepare_init_main_thread()
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
