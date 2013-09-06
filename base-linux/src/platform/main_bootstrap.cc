/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <linux_syscalls.h>


namespace Genode { void platform_main_bootstrap(); }


/*
 * Define 'lx_environ' pointer.
 */
char **lx_environ;


/**
 * Natively aligned memory location used in the lock implementation
 */
int main_thread_futex_counter __attribute__((aligned(sizeof(Genode::addr_t))));


/**
 * Initial value of SP register (in crt0)
 */
extern Genode::addr_t *__initial_sp;


/**
 * Platform-specific bootstrap
 */
void Genode::platform_main_bootstrap()
{
	static struct Bootstrap
	{
		Bootstrap()
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
		}
	} bootstrap;
}
