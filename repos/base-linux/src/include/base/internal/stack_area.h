/*
 * \brief  Linux-specific definitions and utilities for the stack area
 * \author Christian Helmuth
 * \date   2013-09-26
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__STACK_AREA_H_
#define _INCLUDE__BASE__INTERNAL__STACK_AREA_H_

/* Genode includes */
#include <base/thread.h>
#include <rm_session/rm_session.h>

/* Linux includes */
#include <linux_syscalls.h>


extern Genode::addr_t _stack_area_start;


namespace Genode {

	/**
	 * Stack area base address
	 *
	 * Please update platform-specific files after changing these
	 * functions, e.g., 'base-linux/src/ld/stack_area.ld'.
	 */
	static addr_t stack_area_virtual_base() {
		return align_addr((addr_t)&_stack_area_start, 20); }

	static constexpr addr_t stack_area_virtual_size() { return 0x10000000UL; }
	static constexpr addr_t stack_virtual_size()      { return 0x00100000UL; }
}


static inline void flush_stack_area()
{
	using namespace Genode;

	void         * const base = (void *)stack_area_virtual_base();
	Genode::size_t const size = stack_area_virtual_size();

	int ret;
	if ((ret = lx_munmap(base, size)) < 0) {
		error(__func__, ": failed ret=", ret);
		throw Region_map::Region_conflict();
	}
}


static inline Genode::addr_t reserve_stack_area()
{
	using namespace Genode;
	using Genode::size_t;

	int const flags       = MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED;
	int const prot        = PROT_NONE;
	size_t const size     = stack_area_virtual_size();
	void * const addr_in  = (void *)stack_area_virtual_base();
	void * const addr_out = lx_mmap(addr_in, size, prot, flags, -1, 0);

	/* reserve at local address failed - unmap incorrect mapping */
	if (addr_in != addr_out) {
		lx_munmap((void *)addr_out, size);
		error(__func__, ": failed addr_in=", addr_in, " addr_out=", addr_out);
		throw Region_map::Region_conflict();
	}

	return (addr_t) addr_out;
}

#endif /* _INCLUDE__BASE__INTERNAL__STACK_AREA_H_ */
