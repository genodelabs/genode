/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-12-28
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM___MAIN_HELPER_H_
#define _PLATFORM___MAIN_HELPER_H_

#include <nova/syscalls.h>

/**
 * Location of the main thread's UTCB, initialized by the startup code
 */
extern Nova::mword_t __main_thread_utcb;

/**
 * Initial value of esp register, saved by the crt0 startup code
 *
 * This value contains the address of the hypervisor information page.
 */
extern long __initial_sp;

/**
 * First available capability selector for custom use
 */
extern int __first_free_cap_selector;

/**
 * Selector of local protection domain
 */
extern int __local_pd_sel;

static void main_thread_bootstrap()
{
	/* register UTCB of main thread */
	__main_thread_utcb = __initial_sp - Nova::PAGE_SIZE_BYTE;

	/* register start of usable capability range */
	enum { FIRST_FREE_PORTAL = 0x1000 };

	/* this variable may be set by the dynamic linker (ldso) */
	if (!__first_free_cap_selector)
		__first_free_cap_selector = FIRST_FREE_PORTAL;

	/* register pd selector at cap allocator */
	__local_pd_sel = Nova::PD_SEL;
}

#endif /* _PLATFORM___MAIN_HELPER_H_ */
