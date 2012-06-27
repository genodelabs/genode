/*
 * \brief  Helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2009-10-02
 *
 * For documentation about the interface, please revisit the 'base-pistachio'
 * implementation.
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>
#include <base/thread.h>

/* NOVA includes */
#include <nova/syscalls.h>


extern int main_thread_running_semaphore();


/**
 * Resolve 'Thread_base::myself' when not linking the thread library
 *
 * This weak symbol is primarily used by test cases. Most other Genode programs
 * use the thread library. If the thread library is not used, 'myself' can only
 * be called by the main thread, for which 'myself' is defined as zero.
 */
Genode::Thread_base * __attribute__((weak)) Genode::Thread_base::myself() { return 0; }


static inline void thread_yield() { }


static bool thread_check_stopped_and_restart(Genode::Native_thread_id tid)
{
	int sem = tid.rs_sel == 0 ? main_thread_running_semaphore()
	                          : tid.rs_sel;

	Nova::sm_ctrl(sem, Nova::SEMAPHORE_UP);
	return true;
}


static inline Genode::Native_thread_id thread_get_my_native_id()
{
	/*
	 * We encode the main thread as tid { 0, 0, 0 } because we cannot
	 * call 'main_thread_running_semaphore()' here.
	 */
	Genode::Thread_base *myself = Genode::Thread_base::myself();

	if (myself == 0) {
		Genode::Native_thread_id main_tid = { 0, 0, 0 };
		return main_tid;
	} else
		return myself->tid();
}


static inline Genode::Native_thread_id thread_invalid_id()
{
	Genode::Native_thread_id tid = { 0, 0, -1 };
	return tid;
}


static inline bool thread_id_valid(Genode::Native_thread_id tid)
{
	return tid.rs_sel != -1;
}


static inline void thread_switch_to(Genode::Native_thread_id tid) { }


static inline void thread_stop_myself()
{
	Genode::Thread_base *myself = Genode::Thread_base::myself();
	int sem = myself ? myself->tid().rs_sel : main_thread_running_semaphore();
	Nova::sm_ctrl(sem, Nova::SEMAPHORE_DOWNZERO);
}
