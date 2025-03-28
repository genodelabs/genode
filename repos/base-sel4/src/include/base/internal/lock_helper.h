/*
 * \brief  seL4-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2015-05-07
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

#include <base/log.h>
#include <base/thread.h>

/* seL4 includes */
#include <base/internal/capability_space_sel4.h>
#include <sel4/sel4.h>


static inline void thread_yield() { seL4_Yield(); }


static inline void thread_switch_to(Genode::Thread *)
{
	Genode::warning(__FUNCTION__, " not implemented");
}


static inline unsigned sel4_lock_sel(Genode::Thread *thread_ptr)
{
	if (!thread_ptr)
		return Genode::INITIAL_SEL_LOCK; /* main thread */

	return thread_ptr->with_native_thread(
		[&] (Genode::Native_thread &nt) { return nt.attr.lock_sel; },
		[&] () -> unsigned              { return Genode::INITIAL_SEL_LOCK; });
}


static inline bool thread_check_stopped_and_restart(Genode::Thread *thread_ptr)
{
	seL4_Signal(sel4_lock_sel(thread_ptr));
	return true;
}


static inline void thread_stop_myself(Genode::Thread *myself_ptr)
{
	seL4_Word sender = ~0U;
	seL4_Wait(sel4_lock_sel(myself_ptr), &sender);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
