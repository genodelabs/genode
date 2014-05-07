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
}
