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
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_
#define _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_

/* Genode includes */
#include <base/thread.h>
#include <base/stdint.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* NOVA includes */
#include <nova/syscalls.h>


extern int main_thread_running_semaphore();


static inline Genode::addr_t sm_sel_ec(Genode::Thread *thread_ptr)
{
	if (!thread_ptr)
		return main_thread_running_semaphore();

	using namespace Genode;

	return thread_ptr->with_native_thread(
		[&] (Native_thread &nt) { return nt.exc_pt_sel + Nova::SM_SEL_EC; },
		[&] { error("attempt to synchronize invalid thread"); return 0UL; });
}


static inline bool thread_check_stopped_and_restart(Genode::Thread *thread_ptr)
{
	Nova::sm_ctrl(sm_sel_ec(thread_ptr), Nova::SEMAPHORE_UP);
	return true;
}


static inline void thread_switch_to(Genode::Thread *) { }


static inline void thread_stop_myself(Genode::Thread *myself)
{
	Nova::sm_ctrl(sm_sel_ec(myself), Nova::SEMAPHORE_DOWNZERO);
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
