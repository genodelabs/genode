/*
 * \brief  Minimal support for FreeBSD-specific syscalls
 * \author Christian Helmuth
 * \date   2018-05-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* libc includes */
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>


static int sys_thr_self()
{
	using Genode::addr_t;

	addr_t const base  = Genode::Thread::stack_area_virtual_base();
	addr_t const size  = Genode::Thread::stack_virtual_size();
	addr_t const stack = (addr_t)Genode::Thread::myself()->stack_base();

	return int((stack - base) / size + 1);
}


extern "C" int syscall(int nr, ...)
{
	switch (nr) {
	case SYS_thr_self: return sys_thr_self();

	default:
		errno = ENOSYS;
		return -1;
	}
}
