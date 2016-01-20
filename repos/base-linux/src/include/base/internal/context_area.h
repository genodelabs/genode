/*
 * \brief  Linux-specific utilities for context area
 * \author Christian Helmuth
 * \date   2013-09-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__ENV__CONTEXT_AREA_H_
#define _BASE__ENV__CONTEXT_AREA_H_

/* Genode includes */
#include <base/thread.h>
#include <rm_session/rm_session.h>

#include <linux_syscalls.h>

/* Linux includes */
#include <sys/mman.h>


static inline void flush_context_area()
{
	using namespace Genode;

	void * const base = (void *) Native_config::context_area_virtual_base();
	size_t const size = Native_config::context_area_virtual_size();

	int ret;
	if ((ret = lx_munmap(base, size)) < 0) {
		PERR("%s: failed ret=%d", __func__, ret);
		throw Rm_session::Region_conflict();
	}
}


static inline Genode::addr_t reserve_context_area()
{
	using namespace Genode;

	int const flags       = MAP_ANONYMOUS | MAP_PRIVATE;
	int const prot        = PROT_NONE;
	size_t const size     = Native_config::context_area_virtual_size();
	void * const addr_in  = (void *)Native_config::context_area_virtual_base();
	void * const addr_out = lx_mmap(addr_in, size, prot, flags, -1, 0);

	/* reserve at local address failed - unmap incorrect mapping */
	if (addr_in != addr_out) {
		lx_munmap((void *)addr_out, size);

		PERR("%s: failed addr_in=%p addr_out=%p ret=%ld)", __func__,
		     addr_in, addr_out, (long)addr_out);
		throw Rm_session::Region_conflict();
	}

	return (addr_t) addr_out;
}

#endif /* _BASE__ENV__CONTEXT_AREA_H_ */
