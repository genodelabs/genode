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
 * Copyright (C) 2011-2012 Genode Labs GmbH
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
 * Resolve 'Thread_base::myself' when not linking the thread library
 *
 * This weak symbol is primarily used by test cases. Most other Genode programs
 * use the thread library. If the thread library is not used, 'myself' can only
 * be called by the main thread, for which 'myself' is defined as zero.
 */
Genode::Thread_base * __attribute__((weak)) Genode::Thread_base::myself() { return 0; }

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
static inline bool thread_check_stopped_and_restart(Genode::Native_thread_id tid)
{
	Genode::Native_thread_id irq = tid + Fiasco::THREAD_IRQ_CAP;
	Fiasco::l4_irq_trigger(irq);
	return true;
}


static inline Genode::Native_thread_id thread_get_my_native_id()
{
	Genode::Thread_base *myself = Genode::Thread_base::myself();
	return myself ? myself->tid() : Fiasco::MAIN_THREAD_CAP;
}


static inline Genode::Native_thread_id thread_invalid_id()
{
	return Genode::Native_thread();
}


/**
 * Check if a native thread ID is initialized
 *
 * \return true if ID is initialized
 */
static inline bool thread_id_valid(Genode::Native_thread_id tid)
{
	return Fiasco::Capability::valid(tid);
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Native_thread_id tid)
{
	Fiasco::l4_thread_switch(tid);
}


/**
 * Unconditionally block the calling thread
 */
static inline void thread_stop_myself()
{
	using namespace Fiasco;

	Genode::Native_thread_id irq = thread_get_my_native_id() + THREAD_IRQ_CAP;
	l4_irq_receive(irq, L4_IPC_NEVER);
}

#endif /* _INCLUDE__BASE__LOCK__LOCK_HELPER_H_ */
