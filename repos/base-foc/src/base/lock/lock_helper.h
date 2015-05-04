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
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__LOCK__LOCK_HELPER_H_
#define _INCLUDE__BASE__LOCK__LOCK_HELPER_H_

/* Genode includes */
#include <base/native_types.h>
#include <base/thread.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/kdebug.h>
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
static inline bool thread_check_stopped_and_restart(Genode::Thread_base *thread_base)
{
	Genode::Native_thread_id tid = thread_base ?
	                               thread_base->tid() :
	                               Fiasco::MAIN_THREAD_CAP;
	Genode::Native_thread_id irq = tid + Fiasco::THREAD_IRQ_CAP;
	Fiasco::l4_irq_trigger(irq);
	return true;
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Thread_base *thread_base)
{
	Genode::Native_thread_id tid = thread_base ?
	                               thread_base->tid() :
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
static void thread_stop_myself()
{
	using namespace Fiasco;

	Genode::Thread_base *myself = Genode::Thread_base::myself();
	Genode::Native_thread_id tid = myself ?
	                               myself->tid() :
	                               Fiasco::MAIN_THREAD_CAP;
	Genode::Native_thread_id irq = tid + THREAD_IRQ_CAP;
	l4_irq_receive(irq, L4_IPC_NEVER);
}

#endif /* _INCLUDE__BASE__LOCK__LOCK_HELPER_H_ */
