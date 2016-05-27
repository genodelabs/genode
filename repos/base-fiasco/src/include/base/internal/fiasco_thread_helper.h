/*
 * \brief  Fiasco-specific thread helper functions
 * \author Norman Feske
 * \date   2007-05-03
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FIASCO__THREAD_HELPER_H_
#define _INCLUDE__FIASCO__THREAD_HELPER_H_

#include <base/printf.h>

namespace Fiasco {
#include <l4/sys/types.h>

	inline void print_l4_threadid(l4_threadid_t t)
	{
		Genode::printf("THREAD %x.%02x\n", t.id.task, t.id.lthread);
		Genode::printf("  unsigned version_low:10  = %x\n", t.id.version_low);
		Genode::printf("  unsigned lthread:7       = %x\n", t.id.lthread);
		Genode::printf("  unsigned task:11         = %x\n", t.id.task);
	}


	/**
	 * Sigma0 thread ID
	 *
	 * We must use a raw hex value initializer since we're using C++ and
	 * l4_threadid_t is an union.
	 */
	const l4_threadid_t sigma0_threadid = { 0x00040000 };
}

#endif /* _INCLUDE__FIASCO__THREAD_HELPER_H_ */
