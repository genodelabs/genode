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
namespace Fiasco {
#include <l4/sys/irq.h>
#include <l4/sys/thread.h>
}


/**
 * Yield CPU time
 */
static inline void thread_yield() { Fiasco::l4_thread_yield(); }


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
	Fiasco::l4_cap_idx_t tid = thread_base ?
	                           thread_base->native_thread().kcap :
	                           Fiasco::MAIN_THREAD_CAP;
	Fiasco::l4_cap_idx_t irq = tid + Fiasco::THREAD_IRQ_CAP;
	Fiasco::l4_irq_trigger(irq);
	return true;
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Thread *thread_base)
{
	Fiasco::l4_cap_idx_t tid = thread_base ?
	                           thread_base->native_thread().kcap :
	                           Fiasco::MAIN_THREAD_CAP;
	Fiasco::l4_thread_switch(tid);
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
	using namespace Fiasco;

	Genode::Thread *myself = Genode::Thread::myself();
	Fiasco::l4_cap_idx_t tid = myself ?
	                           myself->native_thread().kcap :
	                           Fiasco::MAIN_THREAD_CAP;
	Fiasco::l4_cap_idx_t irq = tid + THREAD_IRQ_CAP;
	l4_irq_receive(irq, L4_IPC_NEVER);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
