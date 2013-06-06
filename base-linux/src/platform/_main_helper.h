/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Christian Prochaska
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM___MAIN_HELPER_H_
#define _PLATFORM___MAIN_HELPER_H_

#include <base/thread.h>

#include <linux_syscalls.h>

/*
 * Define 'lx_environ' pointer.
 */
char **lx_environ;


/**
 * Natively aligned memory location used in the lock implementation
 */
int main_thread_futex_counter __attribute__((aligned(sizeof(Genode::addr_t))));


static inline void main_thread_bootstrap()
{
	using namespace Genode;

	extern Genode::addr_t *__initial_sp;

	/*
	 * Initialize the 'lx_environ' pointer
	 *
	 * environ = &argv[argc + 1]
	 * __initial_sp[0] = argc (always 1 in Genode)
	 * __initial_sp[1] = argv[0]
	 * __initial_sp[2] = NULL
	 * __initial_sp[3] = environ
	 *
	 */
	lx_environ = (char**)&__initial_sp[3];

	/* reserve context area */
	Genode::addr_t base = Native_config::context_area_virtual_base();
	Genode::size_t size = Native_config::context_area_virtual_size();
	if (lx_vm_reserve(base, size) != base)
		PERR("reservation of context area [%lx,%lx) failed",
		     (unsigned long) base, (unsigned long) base + size);
}

#endif /* _PLATFORM___MAIN_HELPER_H_ */
