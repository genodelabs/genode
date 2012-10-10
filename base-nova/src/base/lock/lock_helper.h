/*
 * \brief  Helper functions for the Lock implementation
 * \author Norman Feske
 * \author Alexander Boettcher
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
#include <base/stdint.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>


extern int main_thread_running_semaphore();


/**
 * Resolve 'Thread_base::myself' when not linking the thread library
 *
 * This weak symbol is primarily used by test cases. Most other Genode programs
 * use the thread library. If the thread library is not used, 'myself' can only
 * be called by the main thread, for which 'myself' is defined as zero.
 */
Genode::Thread_base * __attribute__((weak)) Genode::Thread_base::myself()
{
	return 0;
}


static inline void thread_yield() { }


static inline bool thread_check_stopped_and_restart(Genode::Native_thread_id tid)
{
	Genode::addr_t sem = (tid.ec_sel == 0 && tid.exc_pt_sel == 0) ?
	               main_thread_running_semaphore() :
	               tid.exc_pt_sel + Nova::SM_SEL_EC;

	Nova::sm_ctrl(sem, Nova::SEMAPHORE_UP);
	return true;
}


static inline Genode::Native_thread_id thread_get_my_native_id()
{
	/*
	 * We encode the main thread as tid { 0, 0 } because we cannot
	 * call 'main_thread_running_semaphore()' here.
	 */
	Genode::Thread_base *myself = Genode::Thread_base::myself();

	if (myself == 0) {
		Genode::Native_thread_id main_tid;
		main_tid.ec_sel     = 0;
		main_tid.exc_pt_sel = 0;
		return main_tid;
	} else
		return myself->tid();
}


static inline Genode::Native_thread_id thread_invalid_id()
{
	Genode::Native_thread_id tid;
	return tid;
}


static inline bool thread_id_valid(Genode::Native_thread_id tid)
{
	return !(tid.ec_sel == ~0UL && tid.exc_pt_sel == ~0UL);
}


static inline void thread_switch_to(Genode::Native_thread_id tid) { }


static inline void thread_stop_myself()
{
	using namespace Genode;
	using namespace Nova;

	addr_t sem;
	Thread_base *myself = Thread_base::myself();
	if (myself)
		sem = myself->tid().exc_pt_sel + SM_SEL_EC;
	else
		sem = main_thread_running_semaphore();

	if (sm_ctrl(sem, SEMAPHORE_DOWNZERO))
		nova_die();
}
