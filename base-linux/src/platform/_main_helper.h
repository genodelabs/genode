/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Christian Prochaska
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM___MAIN_HELPER_H_
#define _PLATFORM___MAIN_HELPER_H_

#include <base/thread.h>

#include <linux_syscalls.h>

/*
 * Define 'lx_environ' pointer that is supposed to be initialized by the
 * startup code.
 */
__attribute__((weak)) char **lx_environ = (char **)0;


static inline void main_thread_bootstrap()
{
	using namespace Genode;

	/* reserve context area */
	Genode::addr_t base = Native_config::context_area_virtual_base();
	Genode::size_t size = Native_config::context_area_virtual_size();
	if (lx_vm_reserve(base, size) != base)
		PERR("reservation of context area [%lx,%lx) failed",
		     (unsigned long) base, (unsigned long) base + size);
}

#endif /* _PLATFORM___MAIN_HELPER_H_ */
