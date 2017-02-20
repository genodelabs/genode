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

/* Pistachio includes */
namespace Pistachio {
#include <l4/schedule.h>
#include <l4/ipc.h>
}


extern Pistachio::L4_ThreadId_t main_thread_tid;


/**
 * Yield CPU time
 */
static inline void thread_yield() { Pistachio::L4_Yield(); }


/**
 * Custom ExchangeRegisters wrapper for waking up a thread
 *
 * When waking up an lock applicant, we need to make sure that the thread was
 * stopped beforehand. Therefore, we evaluate the previous thread state as
 * returned by the 'L4_ExchangeRegisters' call.
 *
 * \return true if the thread was in blocking state
 */
static inline bool thread_check_stopped_and_restart(Genode::Thread *thread_base)
{
	using namespace Pistachio;

	L4_Word_t dummy;
	L4_ThreadId_t dummy_id;
	L4_ThreadState_t state;

	Pistachio::L4_ThreadId_t tid = thread_base ?
	                               thread_base->native_thread().l4id :
	                               main_thread_tid;

	enum { RESUME = 1 << 8, CANCEL_IPC = 3 << 1 };
	L4_ExchangeRegisters(tid, RESUME | CANCEL_IPC, 0, 0, 0,
	                     0, L4_nilthread, &state.raw, &dummy, &dummy, &dummy,
	                     &dummy, &dummy_id);

	return L4_ThreadWasHalted(state);
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Thread *thread_base)
{
	Pistachio::L4_ThreadId_t tid = thread_base ?
	                               thread_base->native_thread().l4id :
	                               main_thread_tid;
	Pistachio::L4_ThreadSwitch(tid);
}


/**
 * Unconditionally block the calling thread
 */
static inline void thread_stop_myself()
{
	Genode::Thread *myself = Genode::Thread::myself();
	Pistachio::L4_ThreadId_t tid = myself ?
	                               myself->native_thread().l4id :
	                               main_thread_tid;
	Pistachio::L4_Stop(tid);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
