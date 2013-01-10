/*
 * \brief  Dummy helper functions for the Lock implementation
 * \author Norman Feske
 * \date   2009-10-02
 *
 * For documentation about the interface, please revisit the 'base-pistachio'
 * implementation.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>
#include <base/thread.h>

/* kernel includes */
#include <kernel/syscalls.h>
#include <base/capability.h>


static inline void thread_yield() { Kernel::thread_yield(); }


static bool thread_check_stopped_and_restart(Genode::Native_thread_id tid)
{
	Kernel::thread_wake(tid);
	return true;
}


static inline Genode::Native_thread_id thread_get_my_native_id()
{
	return Genode::my_thread_id();
}


static inline Genode::Native_thread_id thread_invalid_id() { return -1; }


static inline bool thread_id_valid(Genode::Native_thread_id tid)
{
	return tid != thread_invalid_id();
}


static inline void thread_switch_to(Genode::Native_thread_id tid)
{
	thread_yield();
}


static inline void thread_stop_myself() { Kernel::thread_sleep(); }
