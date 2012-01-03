/*
 * \brief  Pistachio-specific helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2009-07-15
 *
 * This file serves as adapter between the generic lock implementation
 * in 'lock.cc' and the underlying kernel.
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/schedule.h>
#include <l4/ipc.h>
}


bool operator == (Genode::Native_thread_id t1, Genode::Native_thread_id t2) { return t1.raw == t2.raw; }
bool operator != (Genode::Native_thread_id t1, Genode::Native_thread_id t2) { return t1.raw != t2.raw; }


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
static bool thread_check_stopped_and_restart(Genode::Native_thread_id tid)
{
	using namespace Pistachio;

	L4_Word_t dummy;
	L4_ThreadId_t dummy_id;
	L4_ThreadState_t state;

	enum { RESUME = 1 << 8, CANCEL_IPC = 3 << 1 };
	L4_ExchangeRegisters(tid, RESUME | CANCEL_IPC, 0, 0, 0,
	                     0, L4_nilthread, &state.raw, &dummy, &dummy, &dummy,
	                     &dummy, &dummy_id);

	return L4_ThreadWasHalted(state);
}


static inline Genode::Native_thread_id thread_get_my_native_id()
{
	return Pistachio::L4_Myself();
}


static inline Genode::Native_thread_id thread_invalid_id()
{
	using namespace Pistachio;
	return L4_nilthread;
}


/**
 * Check if a native thread ID is initialized
 *
 * \return true if ID is initialized
 */
static inline bool thread_id_valid(Genode::Native_thread_id tid)
{
	return (tid.raw != 0);
}


/**
 * Yield CPU time to the specified thread
 */
static inline void thread_switch_to(Genode::Native_thread_id tid)
{
	Pistachio::L4_ThreadSwitch(tid);
}


/**
 * Unconditionally block the calling thread
 */
static inline void thread_stop_myself()
{
	Pistachio::L4_Stop(thread_get_my_native_id());
}
