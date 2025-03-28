/*
 * \brief  Fiasco.OC-specific helper functions for the Lock implementation
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2011-02-22
 *
 * This file serves as adapter between the generic lock implementation
 * in 'lock.cc' and the underlying kernel.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

/* Genode includes */
#include <base/thread.h>
#include <foc/native_capability.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>


/**
 * Yield CPU time
 */
static inline void thread_yield() { Foc::l4_thread_yield(); }


static inline Foc::l4_cap_idx_t foc_cap_idx(Genode::Thread *thread_ptr)
{
	if (!thread_ptr)
		return Foc::MAIN_THREAD_CAP;

	return thread_ptr->with_native_thread(
		[&] (Genode::Native_thread &nt) { return nt.kcap; },
		[&]                             { return Foc::MAIN_THREAD_CAP; });
}


/**
 * Custom ExchangeRegisters wrapper for waking up a thread
 *
 * When waking up an lock applicant, we need to make sure that the thread was
 * stopped beforehand. Therefore, we evaluate the previous thread state as
 * returned by the 'L4_ExchangeRegisters' call.
 *
 * \return true if the thread was in blocking state
 */
static inline bool thread_check_stopped_and_restart(Genode::Thread *thread_ptr)
{
	Foc::l4_irq_trigger(foc_cap_idx(thread_ptr) + Foc::THREAD_IRQ_CAP);
	return true;
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Thread *thread_ptr)
{
	Foc::l4_thread_switch(foc_cap_idx(thread_ptr));
}


/**
 * Unconditionally block the calling thread
 */

/* Build with frame pointer to make GDB backtraces work. See issue #1061. */
__attribute__((optimize("-fno-omit-frame-pointer")))
__attribute__((noinline))
__attribute__((used))
static void thread_stop_myself(Genode::Thread *)
{
	using namespace Foc;
	l4_irq_receive(foc_cap_idx(Genode::Thread::myself()) + THREAD_IRQ_CAP,
	                    L4_IPC_NEVER);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
