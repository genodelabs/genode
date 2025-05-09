/*
 * \brief  Linux-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2009-07-20
 *
 * This file serves as adapter between the generic lock implementation
 * in 'lock.cc' and the underlying kernel.
 *
 * For documentation about the interface, please revisit the 'base-pistachio'
 * implementation.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

/* Genode includes */
#include <base/thread.h>

/* Linux includes */
#include <linux_syscalls.h>


extern int main_thread_futex_counter;


static inline void thread_yield()
{
	struct timespec ts = { 0, 1000 };
	lx_nanosleep(&ts, 0);
}


static inline int *futex_counter_ptr(Genode::Thread *thread_ptr)
{
	if (!thread_ptr)
		return &main_thread_futex_counter;

	return thread_ptr->with_native_thread(
		[&] (Genode::Native_thread &nt) { return &nt.futex_counter; },
		[&] {
			Genode::error("attempt to access futex of invalid thread");
			return (int *)nullptr;
		});
}


static inline bool thread_check_stopped_and_restart(Genode::Thread *thread_ptr)
{
	return lx_futex(futex_counter_ptr(thread_ptr), LX_FUTEX_WAKE, 1);
}


static inline void thread_switch_to(Genode::Thread *) { thread_yield(); }


static inline void thread_stop_myself(Genode::Thread *myself)
{
	/*
	 * Just go to sleep without modifying the counter value. The
	 * 'thread_check_stopped_and_restart()' function will get called
	 * repeatedly until this thread has actually executed the syscall.
	 */
	lx_futex(futex_counter_ptr(myself), LX_FUTEX_WAIT, 0);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
