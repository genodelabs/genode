/*
 * \brief  OKL4-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2009-07-09
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
#include <base/internal/okl4.h>


/**
 * Yield CPU time
 */
static inline void thread_yield() { Okl4::L4_Yield(); }


extern Okl4::L4_ThreadId_t main_thread_tid;


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
	using namespace Okl4;

	L4_Word_t dummy;
	L4_ThreadId_t dummy_id;
	L4_ThreadState_t state;

	Okl4::L4_ThreadId_t tid = thread_base ?
	                          thread_base->native_thread().l4id :
	                          main_thread_tid;

	L4_ExchangeRegisters(tid, L4_ExReg_Resume + L4_ExReg_AbortIPC, 0, 0, 0,
	                     0, L4_nilthread, &state.raw, &dummy, &dummy, &dummy,
	                     &dummy, &dummy_id);

	return L4_ThreadWasHalted(state);
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Thread *thread_base)
{
	Okl4::L4_ThreadId_t tid = thread_base ?
	                          thread_base->native_thread().l4id :
	                          main_thread_tid;
	Okl4::L4_ThreadSwitch(tid);
}


/**
 * Unconditionally block the calling thread
 */
static inline void thread_stop_myself(Genode::Thread *myself)
{
	Okl4::L4_ThreadId_t tid = myself ?
	                          myself->native_thread().l4id :
	                          main_thread_tid;
	Okl4::L4_Stop(tid);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
