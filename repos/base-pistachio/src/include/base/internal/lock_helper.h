/*
 * \brief  Pistachio-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2009-07-15
 *
 * This file serves as adapter between the generic lock implementation
 * in 'lock.cc' and the underlying kernel.
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

/* base-internal includes */
#include <base/internal/pistachio.h>


extern Pistachio::L4_ThreadId_t main_thread_tid;


/**
 * Yield CPU time
 */
static inline void thread_yield() { Pistachio::L4_Yield(); }


static inline Pistachio::L4_ThreadId_t pistachio_tid(Genode::Thread *thread_ptr)
{
	if (!thread_ptr)
		return main_thread_tid;

	return thread_ptr->with_native_thread(
		[&] (Genode::Native_thread &nt) { return nt.l4id; },
		[&]                             { return Pistachio::L4_ThreadId_t { }; });
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
	using namespace Pistachio;

	L4_Word_t        dummy;
	L4_ThreadId_t    dummy_id;
	L4_ThreadState_t state;

	enum { RESUME = 1 << 8, CANCEL_IPC = 3 << 1 };
	L4_ExchangeRegisters(pistachio_tid(thread_ptr), RESUME | CANCEL_IPC, 0, 0, 0, 0,
	                     L4_nilthread, &state.raw, &dummy, &dummy, &dummy,
	                     &dummy, &dummy_id);

	return L4_ThreadWasHalted(state);
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Thread *thread_ptr)
{
	Pistachio::L4_ThreadSwitch(pistachio_tid(thread_ptr));
}


/**
 * Unconditionally block the calling thread
 */
static inline void thread_stop_myself(Genode::Thread *thread_ptr)
{
	Pistachio::L4_Stop(pistachio_tid(thread_ptr));
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
