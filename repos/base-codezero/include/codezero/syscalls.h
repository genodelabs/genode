/*
 * \brief  Aggregate Codezero syscall bindings
 * \author Norman Feske
 * \date   2010-02-16
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CODEZERO__SYSCALLS_H_
#define _INCLUDE__CODEZERO__SYSCALLS_H_

/*
 * Codezero headers happen to include the compiler's 'stdarg.h'. If this
 * happened within the 'Codezero' namespace below, we would not be able to
 * include 'stdarg.h' later on into the root namespace (stdarg's include guards
 * would prevent this. Therefore, we make sure to include the file into the
 * root namespace prior processing any Codezero headers.
 */
#include <stdarg.h>

namespace Codezero { extern "C" {

/* make Codezero includes happy */
extern char *strncpy(char *dest, const char *src, __SIZE_TYPE__);
extern void *memcpy(void *dest, const void *src, __SIZE_TYPE__);

/*
 * Work around the problem of C++ keywords being used as
 * argument names in the Codezero API headers.
 */
#define new _new_
#define virtual _virtual_
#define printf(A, ...)

#include <l4lib/macros.h>
#include <l4lib/arch/arm/syscalls.h>
#include <l4lib/arch/arm/syslib.h>
#include <l4lib/ipcdefs.h>
#include <l4lib/init.h>
#include <l4lib/mutex.h>
#include <l4/api/thread.h>
#include <l4/api/irq.h>
#include <l4lib/exregs.h>
#include <l4/lib/list.h>  /* needed for capability.h */
#include <l4/generic/capability.h>
#include <l4/generic/cap-types.h>
#include <l4/arch/arm/exception.h>
#include <l4/arch/arm/io.h>

#undef new
#undef virtual
#ifdef max
#undef max
#endif
#undef printf

/*
 * Turn '#define cacheable' (as defined in the codezero headers) into an enum
 * value. Otherwise, the define will conflict with variables named 'cacheable'.
 */
enum { _codezero_cacheable = cacheable /* #define value */ };
#undef cacheable
enum { cacheable = _codezero_cacheable };

} }

namespace Codezero {

	/**
	 * Return thread ID of the calling thread
	 */
	inline int thread_myself()
	{
		struct task_ids ids = { 0, 0, 0 };
		l4_getid(&ids);
		return ids.tid;
	}
}

#endif /* _INCLUDE__CODEZERO__SYSCALLS_H_ */
