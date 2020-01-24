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
#include <nova/util.h>


extern int main_thread_running_semaphore();


static inline bool thread_check_stopped_and_restart(Genode::Thread *thread_base)
{
	Genode::addr_t sem = thread_base ?
	                     thread_base->native_thread().exc_pt_sel + Nova::SM_SEL_EC :
	                     main_thread_running_semaphore();

	Nova::sm_ctrl(sem, Nova::SEMAPHORE_UP);
	return true;
}


static inline void thread_switch_to(Genode::Thread *) { }


static inline void thread_stop_myself(Genode::Thread *myself)
{
	using namespace Genode;
	using namespace Nova;

	addr_t sem;
	if (myself)
		sem = myself->native_thread().exc_pt_sel + SM_SEL_EC;
	else
		sem = main_thread_running_semaphore();

	if (sm_ctrl(sem, SEMAPHORE_DOWNZERO))
		nova_die();
}

#endif /* _INCLUDE__BASE__INTERNAL__LOCK_HELPER_H_ */
