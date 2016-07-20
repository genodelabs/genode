/*
 * \brief  seL4-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2015-05-07
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

#include <base/log.h>
#include <base/thread.h>

/* seL4 includes */
#include <base/internal/capability_space_sel4.h>
#include <sel4/sel4.h>


static inline void thread_yield() { seL4_Yield(); }


static inline void thread_switch_to(Genode::Thread *thread)
{
	Genode::warning(__FUNCTION__, " not implemented");
}


static inline bool thread_check_stopped_and_restart(Genode::Thread *thread)
{
	unsigned lock_sel = Genode::INITIAL_SEL_LOCK; /* main thread */

	if (thread)
		lock_sel = thread->native_thread().lock_sel;

	seL4_Signal(lock_sel);

	return true;
}


static inline void thread_stop_myself()
{
	Genode::Thread *myself = Genode::Thread::myself();

	unsigned lock_sel = Genode::INITIAL_SEL_LOCK; /* main thread */

	if (myself)
		lock_sel = Genode::Thread::myself()->native_thread().lock_sel;

	seL4_Word sender = ~0U;
	seL4_Wait(lock_sel, &sender);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
