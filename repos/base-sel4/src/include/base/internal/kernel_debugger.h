/*
 * \brief  Platform-specific kernel-debugger hooks for low-level log messages
 * \author Norman Feske
 * \date   2015-05-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__KERNEL_DEBUGGER_H_
#define _INCLUDE__BASE__INTERNAL__KERNEL_DEBUGGER_H_

/* base includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* seL4 includes */
#include <sel4/sel4.h>


static inline void kernel_debugger_outstring(char const *msg)
{
	while (*msg) seL4_DebugPutChar(*msg++);
}


static inline void kernel_debugger_panic(char const *msg)
{
	kernel_debugger_outstring(msg);
	kernel_debugger_outstring("\n");
	seL4_TCB_Suspend(Genode::Thread::myself()->native_thread().tcb_sel);
}

#endif /* _INCLUDE__BASE__INTERNAL__KERNEL_DEBUGGER_H_ */
